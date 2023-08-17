/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file oldloader.cpp Functions for handling of TTO/TTD/TTDP savegames. */

#include "../stdafx.h"
#include "../debug.h"
#include "../strings_type.h"
#include "../string_func.h"
#include "../settings_type.h"
#include "../fileio_func.h"

#include "table/strings.h"

#include "saveload_internal.h"
#include "oldloader.h"


#include "../safeguards.h"

static const int TTO_HEADER_SIZE = 41;
static const int TTD_HEADER_SIZE = 49;
/** The size of the checksum in the name/header of the TTD/TTO savegames. */
static const int HEADER_CHECKSUM_SIZE = 2;

uint32_t _bump_assert_value;

static inline OldChunkType GetOldChunkType(OldChunkType type)     {return (OldChunkType)GB(type, 0, 4);}
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
			Debug(oldloader, 0, "Read past end of file, loading failed");
			throw std::exception();
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
	Works pretty well if you have many zeros behind each other */

	if (ls->chunk_size == 0) {
		/* Read new chunk */
		int8_t new_byte = ReadByteFromFile(ls);

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
	for (const OldChunks *chunk = chunks; chunk->type != OC_END; chunk++) {
		if (((chunk->type & OC_TTD) && _savegame_type == SGT_TTO) ||
				((chunk->type & OC_TTO) && _savegame_type != SGT_TTO)) {
			/* TTD(P)-only chunk, but TTO savegame || TTO-only chunk, but TTD/TTDP savegame */
			continue;
		}

		byte *ptr = (byte*)chunk->ptr;
		if (chunk->type & OC_DEREFERENCE_POINTER) ptr = *(byte**)ptr;

		for (uint i = 0; i < chunk->amount; i++) {
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
						Debug(oldloader, 4, "Assert point: 0x{:X} / 0x{:X}", ls->total_read, (uint)(size_t)chunk->ptr + _bump_assert_value);
						if (ls->total_read != (size_t)chunk->ptr + _bump_assert_value) throw std::exception();
					default: break;
				}
			} else {
				uint64_t res = 0;

				/* Reading from the file: bits 16 to 23 have the FILE type */
				switch (GetOldChunkFileType(chunk->type)) {
					case OC_FILE_I8:  res = (int8_t)ReadByte(ls); break;
					case OC_FILE_U8:  res = ReadByte(ls); break;
					case OC_FILE_I16: res = (int16_t)ReadUint16(ls); break;
					case OC_FILE_U16: res = ReadUint16(ls); break;
					case OC_FILE_I32: res = (int32_t)ReadUint32(ls); break;
					case OC_FILE_U32: res = ReadUint32(ls); break;
					default: NOT_REACHED();
				}

				/* When both pointers are nullptr, we are just skipping data */
				if (base == nullptr && chunk->ptr == nullptr) continue;

				/* Chunk refers to a struct member, get address in base. */
				if (chunk->ptr == nullptr) ptr = (byte *)chunk->offset(base);

				/* Write the data */
				switch (GetOldChunkVarType(chunk->type)) {
					case OC_VAR_I8: *(int8_t  *)ptr = GB(res, 0, 8); break;
					case OC_VAR_U8: *(uint8_t *)ptr = GB(res, 0, 8); break;
					case OC_VAR_I16:*(int16_t *)ptr = GB(res, 0, 16); break;
					case OC_VAR_U16:*(uint16_t*)ptr = GB(res, 0, 16); break;
					case OC_VAR_I32:*(int32_t *)ptr = res; break;
					case OC_VAR_U32:*(uint32_t*)ptr = res; break;
					case OC_VAR_I64:*(int64_t *)ptr = res; break;
					case OC_VAR_U64:*(uint64_t*)ptr = res; break;
					default: NOT_REACHED();
				}

				/* Increase pointer base for arrays when looping */
				if (chunk->amount > 1 && chunk->ptr != nullptr) ptr += CalcOldVarLen(chunk->type);
			}
		}
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
 * @param len   the length of the title to read/checksum
 * @return true iff the title is valid
 */
static bool VerifyOldNameChecksum(char *title, uint len)
{
	uint16_t sum = 0;
	for (uint i = 0; i < len - HEADER_CHECKSUM_SIZE; i++) {
		sum += title[i];
		sum = ROL(sum, 1);
	}

	sum ^= 0xAAAA; // computed checksum

	uint16_t sum2 = title[len - HEADER_CHECKSUM_SIZE]; // checksum in file
	SB(sum2, 8, 8, title[len - HEADER_CHECKSUM_SIZE + 1]);

	return sum == sum2;
}

static std::tuple<SavegameType, std::string> DetermineOldSavegameTypeAndName(FILE *f)
{
	char buffer[std::max(TTO_HEADER_SIZE, TTD_HEADER_SIZE)];
	if (fread(buffer, 1, lengthof(buffer), f) != lengthof(buffer)) {
		return { SGT_INVALID, "(broken) Unable to read file" };
	}

	if (VerifyOldNameChecksum(buffer, TTO_HEADER_SIZE)) {
		return { SGT_TTO, "(TTO)" + StrMakeValid({buffer, TTO_HEADER_SIZE - HEADER_CHECKSUM_SIZE}) };
	}

	if (VerifyOldNameChecksum(buffer, TTD_HEADER_SIZE)) {
		return { SGT_TTD, "(TTD)" + StrMakeValid({buffer, TTD_HEADER_SIZE - HEADER_CHECKSUM_SIZE}) };
	}

	return { SGT_INVALID, "(broken) Unknown" };
}

typedef bool LoadOldMainProc(LoadgameState *ls);

bool LoadOldSaveGame(const std::string &file)
{
	LoadgameState ls;

	Debug(oldloader, 3, "Trying to load a TTD(Patch) savegame");

	InitLoading(&ls);

	/* Open file */
	ls.file = FioFOpenFile(file, "rb", NO_DIRECTORY);

	if (ls.file == nullptr) {
		Debug(oldloader, 0, "Cannot open file '{}'", file);
		return false;
	}

	SavegameType type;
	std::tie(type, std::ignore) = DetermineOldSavegameTypeAndName(ls.file);

	LoadOldMainProc *proc = nullptr;

	switch (type) {
		case SGT_TTO: proc = &LoadTTOMain; break;
		case SGT_TTD: proc = &LoadTTDMain; break;
		default: break;
	}

	_savegame_type = type;

	bool game_loaded;
	try {
		game_loaded = proc != nullptr && proc(&ls);
	} catch (...) {
		game_loaded = false;
	}

	if (!game_loaded) {
		SetSaveLoadError(STR_GAME_SAVELOAD_ERROR_DATA_INTEGRITY_CHECK_FAILED);
		fclose(ls.file);
		return false;
	}

	_pause_mode = PM_PAUSED_SAVELOAD;

	return true;
}

std::string GetOldSaveGameName(const std::string &file)
{
	FILE *f = FioFOpenFile(file, "rb", NO_DIRECTORY);
	if (f == nullptr) return {};

	std::string name;
	std::tie(std::ignore, name) = DetermineOldSavegameTypeAndName(f);
	fclose(f);
	return name;
}
