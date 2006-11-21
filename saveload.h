/* $Id$ */

#ifndef SAVELOAD_H
#define SAVELOAD_H

typedef enum SaveOrLoadResult {
	SL_OK     = 0, // completed successfully
	SL_ERROR  = 1, // error that was caught before internal structures were modified
	SL_REINIT = 2, // error that was caught in the middle of updating game state, need to clear it. (can only happen during load)
} SaveOrLoadResult;

typedef enum SaveOrLoadMode {
	SL_INVALID  = -1,
	SL_LOAD     =  0,
	SL_SAVE     =  1,
	SL_OLD_LOAD =  2,
	SL_PNG      =  3,
	SL_BMP      =  4,
} SaveOrLoadMode;

SaveOrLoadResult SaveOrLoad(const char *filename, int mode);
void WaitTillSaved(void);
void DoExitSave(void);


typedef void ChunkSaveLoadProc(void);
typedef void AutolengthProc(void *arg);

typedef struct {
	uint32 id;
	ChunkSaveLoadProc *save_proc;
	ChunkSaveLoadProc *load_proc;
	uint32 flags;
} ChunkHandler;

typedef struct {
	byte null;
} NullStruct;

typedef enum SLRefType {
	REF_ORDER         = 0,
	REF_VEHICLE       = 1,
	REF_STATION       = 2,
	REF_TOWN          = 3,
	REF_VEHICLE_OLD   = 4,
	REF_ROADSTOPS     = 5,
	REF_ENGINE_RENEWS = 6,
} SLRefType;

#define SL_MAX_VERSION 255

enum {
	INC_VEHICLE_COMMON = 0,
};

enum {
	CH_RIFF         =  0,
	CH_ARRAY        =  1,
	CH_SPARSE_ARRAY =  2,
	CH_TYPE_MASK    =  3,
	CH_LAST         =  8,
	CH_AUTO_LENGTH  = 16,

	CH_PRI_0          = 0 << 4,
	CH_PRI_1          = 1 << 4,
	CH_PRI_2          = 2 << 4,
	CH_PRI_3          = 3 << 4,
	CH_PRI_SHL        = 4,
	CH_NUM_PRI_LEVELS = 4,
};

/** VarTypes is the general bitmasked magic type that tells us
 * certain characteristics about the variable it refers to. For example
 * SLE_FILE_* gives the size(type) as it would be in the savegame and
 * SLE_VAR_* the size(type) as it is in memory during runtime. These are
 * the first 8 bytes (0-3 SLE_FILE, 4-7 SLE_VAR).
 * Bytes 8-15 are reserved for various flags as explained below */
enum VarTypes {
	/* 4 bytes allocated a maximum of 16 types for NumberType */
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

	/* 4 bytes allocated a maximum of 16 types for NumberType */
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
	/* 2 more possible memory-primitives */

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

	/* Shortcut values */
	SLE_UINT  = SLE_UINT32,
	SLE_INT   = SLE_INT32,
	SLE_STRB  = SLE_STRINGBUF,
	SLE_STRBQ = SLE_STRINGBQUOTE,
	SLE_STR   = SLE_STRING,
	SLE_STRQ  = SLE_STRINGQUOTE,

	/* 8 bytes allocated for a maximum of 8 flags
	 * Flags directing saving/loading of a variable */
	SLF_SAVE_NO      = 1 <<  8, ///< do not save with savegame, basically player-based
	SLF_CONFIG_NO    = 1 <<  9, ///< do not save to config file
	SLF_NETWORK_NO   = 1 << 10, ///< do not synchronize over network (but it is saved if SSF_SAVE_NO is not set)
	/* 5 more possible flags */
};

typedef uint32 VarType;

enum SaveLoadTypes {
	SL_VAR       =  0,
	SL_REF       =  1,
	SL_ARR       =  2,
	SL_STR       =  3,
	// non-normal save-load types
	SL_WRITEBYTE =  8,
	SL_INCLUDE   =  9,
	SL_END       = 15
};

typedef byte SaveLoadType;

/** SaveLoad type struct. Do NOT use this directly but use the SLE_ macros defined just below! */
typedef struct SaveLoad {
	SaveLoadType cmd;    ///< the action to take with the saved/loaded type, All types need different action
	VarType conv;        ///< type of the variable to be saved, int
	uint16 length;       ///< (conditional) length of the variable (eg. arrays) (max array size is 65536 elements)
	uint16 version_from; ///< save/load the variable starting from this savegame version
	uint16 version_to;   ///< save/load the variable until this savegame version
	/* NOTE: This element either denotes the address of the variable for a global
	 * variable, or the offset within a struct which is then bound to a variable
	 * during runtime. Decision on which one to use is controlled by the function
	 * that is called to save it. address: SlGlobList, offset: SlObject */
	void *address;       ///< address of variable OR offset of variable in the struct (max offset is 65536)
} SaveLoad;

/* Same as SaveLoad but global variables are used (for better readability); */
typedef SaveLoad SaveLoadGlobVarList;

/* Simple variables, references (pointers) and arrays */
#define SLE_GENERAL(cmd, base, variable, type, length, from, to) {cmd, type, length, from, to, (void*)offsetof(base, variable)}
#define SLE_CONDVAR(base, variable, type, from, to) SLE_GENERAL(SL_VAR, base, variable, type, 0, from, to)
#define SLE_CONDREF(base, variable, type, from, to) SLE_GENERAL(SL_REF, base, variable, type, 0, from, to)
#define SLE_CONDARR(base, variable, type, length, from, to) SLE_GENERAL(SL_ARR, base, variable, type, length, from, to)
#define SLE_CONDSTR(base, variable, type, length, from, to) SLE_GENERAL(SL_STR, base, variable, type, length, from, to)

#define SLE_VAR(base, variable, type) SLE_CONDVAR(base, variable, type, 0, SL_MAX_VERSION)
#define SLE_REF(base, variable, type) SLE_CONDREF(base, variable, type, 0, SL_MAX_VERSION)
#define SLE_ARR(base, variable, type, length) SLE_CONDARR(base, variable, type, length, 0, SL_MAX_VERSION)
#define SLE_STR(base, variable, type, length) SLE_CONDSTR(base, variable, type, length, 0, SL_MAX_VERSION)

#define SLE_CONDNULL(length, from, to) SLE_CONDARR(NullStruct, null, SLE_FILE_U8 | SLE_VAR_NULL | SLF_CONFIG_NO, length, from, to)

/* Translate values ingame to different values in the savegame and vv */
#define SLE_WRITEBYTE(base, variable, game_value, file_value) SLE_GENERAL(SL_WRITEBYTE, base, variable, 0, 0, game_value, file_value)
/* Load common code and put it into each struct (currently only for vehicles */
#define SLE_INCLUDE(base, variable, include_index) SLE_GENERAL(SL_INCLUDE, base, variable, 0, 0, include_index, 0)

/* The same as the ones at the top, only the offset is given directly; used for unions */
#define SLE_GENERALX(cmd, offset, type, param1, param2) {cmd, type, 0, param1, param2, (void*)(offset)}
#define SLE_CONDVARX(offset, type, from, to) SLE_GENERALX(SL_VAR, offset, type, from, to)
#define SLE_CONDREFX(offset, type, from, to) SLE_GENERALX(SL_REF, offset, type, from, to)

#define SLE_VARX(offset, type) SLE_CONDVARX(offset, type, 0, SL_MAX_VERSION)
#define SLE_REFX(offset, type) SLE_CONDREFX(offset, type, 0, SL_MAX_VERSION)

#define SLE_WRITEBYTEX(offset, something) SLE_GENERALX(SL_WRITEBYTE, offset, 0, something, 0)
#define SLE_INCLUDEX(offset, type) SLE_GENERALX(SL_INCLUDE, offset, type, 0, SL_MAX_VERSION)

/* End marker */
#define SLE_END() {SL_END, 0, 0, 0, 0, NULL}

/* Simple variables, references (pointers) and arrays, but for global variables */
#define SLEG_GENERAL(cmd, variable, type, length, from, to) {cmd, type, length, from, to, (void*)&variable}

#define SLEG_CONDVAR(variable, type, from, to) SLEG_GENERAL(SL_VAR, variable, type, 0, from, to)
#define SLEG_CONDREF(variable, type, from, to) SLEG_GENERAL(SL_REF, variable, type, 0, from, to)
#define SLEG_CONDARR(variable, type, length, from, to) SLEG_GENERAL(SL_ARR, variable, type, length, from, to)
#define SLEG_CONDSTR(variable, type, length, from, to) SLEG_GENERAL(SL_STR, variable, type, length, from, to)

#define SLEG_VAR(variable, type) SLEG_CONDVAR(variable, type, 0, SL_MAX_VERSION)
#define SLEG_REF(variable, type) SLEG_CONDREF(variable, type, 0, SL_MAX_VERSION)
#define SLEG_ARR(variable, type) SLEG_CONDARR(variable, type, lengthof(variable), 0, SL_MAX_VERSION)
#define SLEG_STR(variable, type) SLEG_CONDSTR(variable, type, lengthof(variable), 0, SL_MAX_VERSION)

#define SLEG_CONDNULL(length, from, to) {SL_ARR, SLE_FILE_U8 | SLE_VAR_NULL | SLF_CONFIG_NO, length, from, to, (void*)NULL}

#define SLEG_END() {SL_END, 0, 0, 0, 0, NULL}

/** Checks if the savegame is below major.minor.
 */
static inline bool CheckSavegameVersionOldStyle(uint16 major, byte minor)
{
	extern uint16 _sl_version;
	extern byte   _sl_minor_version;
	return (_sl_version < major) || (_sl_version == major && _sl_minor_version < minor);
}

/** Checks if the savegame is below version.
 */
static inline bool CheckSavegameVersion(uint16 version)
{
	extern uint16 _sl_version;
	return _sl_version < version;
}

/** Checks if some version from/to combination falls within the range of the
 * active savegame version */
static inline bool SlIsObjectCurrentlyValid(uint16 version_from, uint16 version_to)
{
	extern const uint16 SAVEGAME_VERSION;
	if (SAVEGAME_VERSION < version_from || SAVEGAME_VERSION > version_to) return false;

	return true;
}

/* Get the NumberType of a setting. This describes the integer type
 * as it is represented in memory
 * @param type VarType holding information about the variable-type
 * @return return the SLE_VAR_* part of a variable-type description */
static inline VarType GetVarMemType(VarType type)
{
	return type & 0xF0; // GB(type, 4, 4) << 4;
}

/* Get the FileType of a setting. This describes the integer type
 * as it is represented in a savegame/file
 * @param type VarType holding information about the variable-type
 * @param return the SLE_FILE_* part of a variable-type description */
static inline VarType GetVarFileType(VarType type)
{
	return type & 0xF; // GB(type, 0, 4);
}

/** Get the address of the variable. Which one to pick depends on the object
 * pointer. If it is NULL we are dealing with global variables so the address
 * is taken. If non-null only the offset is stored in the union and we need
 * to add this to the address of the object */
static inline void *GetVariableAddress(const void *object, const SaveLoad *sld)
{
	return (byte*)object + (ptrdiff_t)sld->address;
}

int64 ReadValue(const void *ptr, VarType conv);
void WriteValue(void *ptr, VarType conv, int64 val);

void SlSetArrayIndex(uint index);
int SlIterateArray(void);

void SlAutolength(AutolengthProc *proc, void *arg);
uint SlGetFieldLength(void);
void SlSetLength(size_t length);
size_t SlCalcObjMemberLength(const SaveLoad *sld);

byte SlReadByte(void);
void SlWriteByte(byte b);

void SlGlobList(const SaveLoadGlobVarList *sldg);
void SlArray(void *array, uint length, VarType conv);
void SlObject(void *object, const SaveLoad *sld);
bool SlObjectMember(void *object, const SaveLoad *sld);

void SaveFileStart(void);
void SaveFileDone(void);
void SaveFileError(void);
#endif /* SAVELOAD_H */
