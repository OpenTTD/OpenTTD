/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload.h Functions/types related to saving and loading games. */

#ifndef SAVELOAD_H
#define SAVELOAD_H

#include "../fileio_type.h"
#include "../strings_type.h"

/** Save or load result codes. */
enum SaveOrLoadResult {
	SL_OK     = 0, ///< completed successfully
	SL_ERROR  = 1, ///< error that was caught before internal structures were modified
	SL_REINIT = 2, ///< error that was caught in the middle of updating game state, need to clear it. (can only happen during load)
};

/** Deals with the type of the savegame, independent of extension */
struct FileToSaveLoad {
	SaveLoadOperation file_op;           ///< File operation to perform.
	DetailedFileType detail_ftype;   ///< Concrete file type (PNG, BMP, old save, etc).
	AbstractFileType abstract_ftype; ///< Abstract type of file (scenario, heightmap, etc).
	char name[MAX_PATH];             ///< Name of the file.
	char title[255];                 ///< Internal name of the game.

	void SetMode(FiosType ft);
	void SetMode(SaveLoadOperation fop, AbstractFileType aft, DetailedFileType dft);
	void SetName(const char *name);
	void SetTitle(const char *title);
};

/** Types of save games. */
enum SavegameType {
	SGT_TTD,    ///< TTD  savegame (can be detected incorrectly)
	SGT_TTDP1,  ///< TTDP savegame ( -//- ) (data at NW border)
	SGT_TTDP2,  ///< TTDP savegame in new format (data at SE border)
	SGT_OTTD,   ///< OTTD savegame
	SGT_TTO,    ///< TTO savegame
	SGT_INVALID = 0xFF, ///< broken savegame (used internally)
};

extern FileToSaveLoad _file_to_saveload;

void GenerateDefaultSaveName(char *buf, const char *last);
void SetSaveLoadError(uint16 str);
const char *GetSaveLoadErrorString();
SaveOrLoadResult SaveOrLoad(const char *filename, SaveLoadOperation fop, DetailedFileType dft, Subdirectory sb, bool threaded = true);
void WaitTillSaved();
void ProcessAsyncSaveFinish();
void DoExitSave();

SaveOrLoadResult SaveWithFilter(struct SaveFilter *writer, bool threaded);
SaveOrLoadResult LoadWithFilter(struct LoadFilter *reader);

typedef void ChunkSaveLoadProc();
typedef void AutolengthProc(void *arg);

/** Handlers and description of chunk. */
struct ChunkHandler {
	uint32 id;                          ///< Unique ID (4 letters).
	ChunkSaveLoadProc *save_proc;       ///< Save procedure of the chunk.
	ChunkSaveLoadProc *load_proc;       ///< Load procedure of the chunk.
	ChunkSaveLoadProc *ptrs_proc;       ///< Manipulate pointers in the chunk.
	ChunkSaveLoadProc *load_check_proc; ///< Load procedure for game preview.
	uint32 flags;                       ///< Flags of the chunk. @see ChunkType
};

struct NullStruct {
	byte null;
};

/** Type of reference (#SLE_REF, #SLE_CONDREF). */
enum SLRefType {
	REF_ORDER          =  0, ///< Load/save a reference to an order.
	REF_VEHICLE        =  1, ///< Load/save a reference to a vehicle.
	REF_STATION        =  2, ///< Load/save a reference to a station.
	REF_TOWN           =  3, ///< Load/save a reference to a town.
	REF_VEHICLE_OLD    =  4, ///< Load/save an old-style reference to a vehicle (for pre-4.4 savegames).
	REF_ROADSTOPS      =  5, ///< Load/save a reference to a bus/truck stop.
	REF_ENGINE_RENEWS  =  6, ///< Load/save a reference to an engine renewal (autoreplace).
	REF_CARGO_PACKET   =  7, ///< Load/save a reference to a cargo packet.
	REF_ORDERLIST      =  8, ///< Load/save a reference to an orderlist.
	REF_STORAGE        =  9, ///< Load/save a reference to a persistent storage.
	REF_LINK_GRAPH     = 10, ///< Load/save a reference to a link graph.
	REF_LINK_GRAPH_JOB = 11, ///< Load/save a reference to a link graph job.
};

/** Highest possible savegame version. */
#define SL_MAX_VERSION UINT16_MAX

/** Flags of a chunk. */
enum ChunkType {
	CH_RIFF         =  0,
	CH_ARRAY        =  1,
	CH_SPARSE_ARRAY =  2,
	CH_TYPE_MASK    =  3,
	CH_LAST         =  8, ///< Last chunk in this array.
	CH_AUTO_LENGTH  = 16,
};

/**
 * VarTypes is the general bitmasked magic type that tells us
 * certain characteristics about the variable it refers to. For example
 * SLE_FILE_* gives the size(type) as it would be in the savegame and
 * SLE_VAR_* the size(type) as it is in memory during runtime. These are
 * the first 8 bits (0-3 SLE_FILE, 4-7 SLE_VAR).
 * Bits 8-15 are reserved for various flags as explained below
 */
enum VarTypes {
	/* 4 bits allocated a maximum of 16 types for NumberType */
	SLE_FILE_I8       = 0,
	SLE_FILE_U8       = 1,
	SLE_FILE_I16      = 2,
	SLE_FILE_U16      = 3,
	SLE_FILE_I32      = 4,
	SLE_FILE_U32      = 5,
	SLE_FILE_I64      = 6,
	SLE_FILE_U64      = 7,
	SLE_FILE_STRINGID = 8, ///< StringID offset into strings-array
	SLE_FILE_STRING   = 9,
	/* 6 more possible file-primitives */

	/* 4 bits allocated a maximum of 16 types for NumberType */
	SLE_VAR_BL    =  0 << 4,
	SLE_VAR_I8    =  1 << 4,
	SLE_VAR_U8    =  2 << 4,
	SLE_VAR_I16   =  3 << 4,
	SLE_VAR_U16   =  4 << 4,
	SLE_VAR_I32   =  5 << 4,
	SLE_VAR_U32   =  6 << 4,
	SLE_VAR_I64   =  7 << 4,
	SLE_VAR_U64   =  8 << 4,
	SLE_VAR_NULL  =  9 << 4, ///< useful to write zeros in savegame.
	SLE_VAR_STRB  = 10 << 4, ///< string (with pre-allocated buffer)
	SLE_VAR_STRBQ = 11 << 4, ///< string enclosed in quotes (with pre-allocated buffer)
	SLE_VAR_STR   = 12 << 4, ///< string pointer
	SLE_VAR_STRQ  = 13 << 4, ///< string pointer enclosed in quotes
	SLE_VAR_NAME  = 14 << 4, ///< old custom name to be converted to a char pointer
	/* 1 more possible memory-primitives */

	/* Shortcut values */
	SLE_VAR_CHAR = SLE_VAR_I8,

	/* Default combinations of variables. As savegames change, so can variables
	 * and thus it is possible that the saved value and internal size do not
	 * match and you need to specify custom combo. The defaults are listed here */
	SLE_BOOL         = SLE_FILE_I8  | SLE_VAR_BL,
	SLE_INT8         = SLE_FILE_I8  | SLE_VAR_I8,
	SLE_UINT8        = SLE_FILE_U8  | SLE_VAR_U8,
	SLE_INT16        = SLE_FILE_I16 | SLE_VAR_I16,
	SLE_UINT16       = SLE_FILE_U16 | SLE_VAR_U16,
	SLE_INT32        = SLE_FILE_I32 | SLE_VAR_I32,
	SLE_UINT32       = SLE_FILE_U32 | SLE_VAR_U32,
	SLE_INT64        = SLE_FILE_I64 | SLE_VAR_I64,
	SLE_UINT64       = SLE_FILE_U64 | SLE_VAR_U64,
	SLE_CHAR         = SLE_FILE_I8  | SLE_VAR_CHAR,
	SLE_STRINGID     = SLE_FILE_STRINGID | SLE_VAR_U16,
	SLE_STRINGBUF    = SLE_FILE_STRING   | SLE_VAR_STRB,
	SLE_STRINGBQUOTE = SLE_FILE_STRING   | SLE_VAR_STRBQ,
	SLE_STRING       = SLE_FILE_STRING   | SLE_VAR_STR,
	SLE_STRINGQUOTE  = SLE_FILE_STRING   | SLE_VAR_STRQ,
	SLE_NAME         = SLE_FILE_STRINGID | SLE_VAR_NAME,

	/* Shortcut values */
	SLE_UINT  = SLE_UINT32,
	SLE_INT   = SLE_INT32,
	SLE_STRB  = SLE_STRINGBUF,
	SLE_STRBQ = SLE_STRINGBQUOTE,
	SLE_STR   = SLE_STRING,
	SLE_STRQ  = SLE_STRINGQUOTE,

	/* 8 bits allocated for a maximum of 8 flags
	 * Flags directing saving/loading of a variable */
	SLF_NOT_IN_SAVE     = 1 <<  8, ///< do not save with savegame, basically client-based
	SLF_NOT_IN_CONFIG   = 1 <<  9, ///< do not save to config file
	SLF_NO_NETWORK_SYNC = 1 << 10, ///< do not synchronize over network (but it is saved if SLF_NOT_IN_SAVE is not set)
	SLF_ALLOW_CONTROL   = 1 << 11, ///< allow control codes in the strings
	SLF_ALLOW_NEWLINE   = 1 << 12, ///< allow new lines in the strings
	/* 3 more possible flags */
};

typedef uint32 VarType;

/** Type of data saved. */
enum SaveLoadTypes {
	SL_VAR         =  0, ///< Save/load a variable.
	SL_REF         =  1, ///< Save/load a reference.
	SL_ARR         =  2, ///< Save/load an array.
	SL_STR         =  3, ///< Save/load a string.
	SL_LST         =  4, ///< Save/load a list.
	/* non-normal save-load types */
	SL_WRITEBYTE   =  8,
	SL_VEH_INCLUDE =  9,
	SL_ST_INCLUDE  = 10,
	SL_END         = 15
};

typedef byte SaveLoadType; ///< Save/load type. @see SaveLoadTypes

/** SaveLoad type struct. Do NOT use this directly but use the SLE_ macros defined just below! */
struct SaveLoad {
	bool global;         ///< should we load a global variable or a non-global one
	SaveLoadType cmd;    ///< the action to take with the saved/loaded type, All types need different action
	VarType conv;        ///< type of the variable to be saved, int
	uint16 length;       ///< (conditional) length of the variable (eg. arrays) (max array size is 65536 elements)
	uint16 version_from; ///< save/load the variable starting from this savegame version
	uint16 version_to;   ///< save/load the variable until this savegame version
	/* NOTE: This element either denotes the address of the variable for a global
	 * variable, or the offset within a struct which is then bound to a variable
	 * during runtime. Decision on which one to use is controlled by the function
	 * that is called to save it. address: global=true, offset: global=false */
	void *address;       ///< address of variable OR offset of variable in the struct (max offset is 65536)
	size_t size;         ///< the sizeof size.
};

/** Same as #SaveLoad but global variables are used (for better readability); */
typedef SaveLoad SaveLoadGlobVarList;

/**
 * Storage of simple variables, references (pointers), and arrays.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @note In general, it is better to use one of the SLE_* macros below.
 */
#define SLE_GENERAL(cmd, base, variable, type, length, from, to) {false, cmd, type, length, from, to, (void*)cpp_offsetof(base, variable), cpp_sizeof(base, variable)}

/**
 * Storage of a variable in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDVAR(base, variable, type, from, to) SLE_GENERAL(SL_VAR, base, variable, type, 0, from, to)

/**
 * Storage of a reference in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Type of the reference, a value from #SLRefType.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDREF(base, variable, type, from, to) SLE_GENERAL(SL_REF, base, variable, type, 0, from, to)

/**
 * Storage of an array in some savegame versions.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLE_CONDARR(base, variable, type, length, from, to) SLE_GENERAL(SL_ARR, base, variable, type, length, from, to)

/**
 * Storage of a string in some savegame versions.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the string (only used for fixed size buffers).
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLE_CONDSTR(base, variable, type, length, from, to) SLE_GENERAL(SL_STR, base, variable, type, length, from, to)

/**
 * Storage of a list in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDLST(base, variable, type, from, to) SLE_GENERAL(SL_LST, base, variable, type, 0, from, to)

/**
 * Storage of a variable in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_VAR(base, variable, type) SLE_CONDVAR(base, variable, type, 0, SL_MAX_VERSION)

/**
 * Storage of a reference in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Type of the reference, a value from #SLRefType.
 */
#define SLE_REF(base, variable, type) SLE_CONDREF(base, variable, type, 0, SL_MAX_VERSION)

/**
 * Storage of an array in every version of a savegame.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLE_ARR(base, variable, type, length) SLE_CONDARR(base, variable, type, length, 0, SL_MAX_VERSION)

/**
 * Storage of a string in every savegame version.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the string (only used for fixed size buffers).
 */
#define SLE_STR(base, variable, type, length) SLE_CONDSTR(base, variable, type, length, 0, SL_MAX_VERSION)

/**
 * Storage of a list in every savegame version.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_LST(base, variable, type) SLE_CONDLST(base, variable, type, 0, SL_MAX_VERSION)

/**
 * Empty space in every savegame version.
 * @param length Length of the empty space.
 */
#define SLE_NULL(length) SLE_CONDNULL(length, 0, SL_MAX_VERSION)

/**
 * Empty space in some savegame versions.
 * @param length Length of the empty space.
 * @param from   First savegame version that has the empty space.
 * @param to     Last savegame version that has the empty space.
 */
#define SLE_CONDNULL(length, from, to) SLE_CONDARR(NullStruct, null, SLE_FILE_U8 | SLE_VAR_NULL | SLF_NOT_IN_CONFIG, length, from, to)

/** Translate values ingame to different values in the savegame and vv. */
#define SLE_WRITEBYTE(base, variable, value) SLE_GENERAL(SL_WRITEBYTE, base, variable, 0, 0, value, value)

#define SLE_VEH_INCLUDE() {false, SL_VEH_INCLUDE, 0, 0, 0, SL_MAX_VERSION, NULL, 0}
#define SLE_ST_INCLUDE() {false, SL_ST_INCLUDE, 0, 0, 0, SL_MAX_VERSION, NULL, 0}

/** End marker of a struct/class save or load. */
#define SLE_END() {false, SL_END, 0, 0, 0, 0, NULL, 0}

/**
 * Storage of global simple variables, references (pointers), and arrays.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @note In general, it is better to use one of the SLEG_* macros below.
 */
#define SLEG_GENERAL(cmd, variable, type, length, from, to) {true, cmd, type, length, from, to, (void*)&variable, sizeof(variable)}

/**
 * Storage of a global variable in some savegame versions.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLEG_CONDVAR(variable, type, from, to) SLEG_GENERAL(SL_VAR, variable, type, 0, from, to)

/**
 * Storage of a global reference in some savegame versions.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLEG_CONDREF(variable, type, from, to) SLEG_GENERAL(SL_REF, variable, type, 0, from, to)

/**
 * Storage of a global array in some savegame versions.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLEG_CONDARR(variable, type, length, from, to) SLEG_GENERAL(SL_ARR, variable, type, length, from, to)

/**
 * Storage of a global string in some savegame versions.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the string (only used for fixed size buffers).
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLEG_CONDSTR(variable, type, length, from, to) SLEG_GENERAL(SL_STR, variable, type, length, from, to)

/**
 * Storage of a global list in some savegame versions.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDLST(variable, type, from, to) SLEG_GENERAL(SL_LST, variable, type, 0, from, to)

/**
 * Storage of a global variable in every savegame version.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_VAR(variable, type) SLEG_CONDVAR(variable, type, 0, SL_MAX_VERSION)

/**
 * Storage of a global reference in every savegame version.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_REF(variable, type) SLEG_CONDREF(variable, type, 0, SL_MAX_VERSION)

/**
 * Storage of a global array in every savegame version.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_ARR(variable, type) SLEG_CONDARR(variable, type, lengthof(variable), 0, SL_MAX_VERSION)

/**
 * Storage of a global string in every savegame version.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_STR(variable, type) SLEG_CONDSTR(variable, type, lengthof(variable), 0, SL_MAX_VERSION)

/**
 * Storage of a global list in every savegame version.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_LST(variable, type) SLEG_CONDLST(variable, type, 0, SL_MAX_VERSION)

/**
 * Empty global space in some savegame versions.
 * @param length Length of the empty space.
 * @param from   First savegame version that has the empty space.
 * @param to     Last savegame version that has the empty space.
 */
#define SLEG_CONDNULL(length, from, to) {true, SL_ARR, SLE_FILE_U8 | SLE_VAR_NULL | SLF_NOT_IN_CONFIG, length, from, to, (void*)NULL}

/** End marker of global variables save or load. */
#define SLEG_END() {true, SL_END, 0, 0, 0, 0, NULL, 0}

/**
 * Checks whether the savegame is below \a major.\a minor.
 * @param major Major number of the version to check against.
 * @param minor Minor number of the version to check against. If \a minor is 0 or not specified, only the major number is checked.
 * @return Savegame version is earlier than the specified version.
 */
static inline bool IsSavegameVersionBefore(uint16 major, byte minor = 0)
{
	extern uint16 _sl_version;
	extern byte   _sl_minor_version;
	return _sl_version < major || (minor > 0 && _sl_version == major && _sl_minor_version < minor);
}

/**
 * Checks if some version from/to combination falls within the range of the
 * active savegame version.
 * @param version_from Lowest version number that falls within the range.
 * @param version_to   Highest version number that falls within the range.
 * @return Active savegame version falls within the given range.
 */
static inline bool SlIsObjectCurrentlyValid(uint16 version_from, uint16 version_to)
{
	extern const uint16 SAVEGAME_VERSION;
	if (SAVEGAME_VERSION < version_from || SAVEGAME_VERSION > version_to) return false;

	return true;
}

/**
 * Get the NumberType of a setting. This describes the integer type
 * as it is represented in memory
 * @param type VarType holding information about the variable-type
 * @return return the SLE_VAR_* part of a variable-type description
 */
static inline VarType GetVarMemType(VarType type)
{
	return type & 0xF0; // GB(type, 4, 4) << 4;
}

/**
 * Get the #FileType of a setting. This describes the integer type
 * as it is represented in a savegame/file
 * @param type VarType holding information about the file-type
 * @param return the SLE_FILE_* part of a variable-type description
 */
static inline VarType GetVarFileType(VarType type)
{
	return type & 0xF; // GB(type, 0, 4);
}

/**
 * Check if the given saveload type is a numeric type.
 * @param conv the type to check
 * @return True if it's a numeric type.
 */
static inline bool IsNumericType(VarType conv)
{
	return GetVarMemType(conv) <= SLE_VAR_U64;
}

/**
 * Get the address of the variable. Which one to pick depends on the object
 * pointer. If it is NULL we are dealing with global variables so the address
 * is taken. If non-null only the offset is stored in the union and we need
 * to add this to the address of the object
 */
static inline void *GetVariableAddress(const void *object, const SaveLoad *sld)
{
	return const_cast<byte *>((const byte*)(sld->global ? NULL : object) + (ptrdiff_t)sld->address);
}

int64 ReadValue(const void *ptr, VarType conv);
void WriteValue(void *ptr, VarType conv, int64 val);

void SlSetArrayIndex(uint index);
int SlIterateArray();

void SlAutolength(AutolengthProc *proc, void *arg);
size_t SlGetFieldLength();
void SlSetLength(size_t length);
size_t SlCalcObjMemberLength(const void *object, const SaveLoad *sld);
size_t SlCalcObjLength(const void *object, const SaveLoad *sld);

byte SlReadByte();
void SlWriteByte(byte b);

void SlGlobList(const SaveLoadGlobVarList *sldg);
void SlArray(void *array, size_t length, VarType conv);
void SlObject(void *object, const SaveLoad *sld);
bool SlObjectMember(void *object, const SaveLoad *sld);
void NORETURN SlError(StringID string, const char *extra_msg = NULL);
void NORETURN SlErrorCorrupt(const char *msg);

bool SaveloadCrashWithMissingNewGRFs();

extern char _savegame_format[8];
extern bool _do_autosave;

#endif /* SAVELOAD_H */
