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
} SaveOrLoadMode;

SaveOrLoadResult SaveOrLoad(const char *filename, int mode);
void WaitTillSaved(void);


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
	REF_ORDER       = 0,
	REF_VEHICLE     = 1,
	REF_STATION     = 2,
	REF_TOWN        = 3,
	REF_VEHICLE_OLD = 4,
	REF_ROADSTOPS   = 5,
	REF_ENGINE_RENEWS = 6,
} SLRefType;

#define SL_MAX_VERSION 255

enum {
	INC_VEHICLE_COMMON = 0,
};

enum {
	CH_RIFF         = 0,
	CH_ARRAY        = 1,
	CH_SPARSE_ARRAY = 2,
	CH_TYPE_MASK    = 3,
	CH_LAST         = 8,
	CH_AUTO_LENGTH  = 16,

	CH_PRI_0          = 0 << 4,
	CH_PRI_1          = 1 << 4,
	CH_PRI_2          = 2 << 4,
	CH_PRI_3          = 3 << 4,
	CH_PRI_SHL        = 4,
	CH_NUM_PRI_LEVELS = 4,
};

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
	SLE_FILE_STRINGID = 8, /// StringID offset into strings-array
	SLE_FILE_STRING   = 9,
	/* 6 more possible primitives */

	/* 4 bytes allocated a maximum of 16 types for NumberType */
	SLE_VAR_BL   =  0 << 4,
	SLE_VAR_I8   =  1 << 4,
	SLE_VAR_U8   =  2 << 4,
	SLE_VAR_I16  =  3 << 4,
	SLE_VAR_U16  =  4 << 4,
	SLE_VAR_I32  =  5 << 4,
	SLE_VAR_U32  =  6 << 4,
	SLE_VAR_I64  =  7 << 4,
	SLE_VAR_U64  =  8 << 4,
	SLE_VAR_NULL =  9 << 4, /// useful to write zeros in savegame.
	SLE_VAR_STRB = 10 << 4, /// normal string (with pre-allocated buffer)
	SLE_VAR_STRQ = 11 << 4, /// string enclosed in parentheses
	/* 4 more possible primitives */

	/* Shortcut values */
	SLE_VAR_CHAR = SLE_VAR_I8,

	/* Default combinations of variables. As savegames change, so can variables
	 * and thus it is possible that the saved value and internal size do not
	 * match and you need to specify custom combo. The defaults are listed here */
	SLE_BOOL        = SLE_FILE_I8  | SLE_VAR_BL,
	SLE_INT8        = SLE_FILE_I8  | SLE_VAR_I8,
	SLE_UINT8       = SLE_FILE_U8  | SLE_VAR_U8,
	SLE_INT16       = SLE_FILE_I16 | SLE_VAR_I16,
	SLE_UINT16      = SLE_FILE_U16 | SLE_VAR_U16,
	SLE_INT32       = SLE_FILE_I32 | SLE_VAR_I32,
	SLE_UINT32      = SLE_FILE_U32 | SLE_VAR_U32,
	SLE_INT64       = SLE_FILE_I64 | SLE_VAR_I64,
	SLE_UINT64      = SLE_FILE_U64 | SLE_VAR_U64,
	SLE_CHAR        = SLE_FILE_I8  | SLE_VAR_CHAR,
	SLE_STRINGID    = SLE_FILE_STRINGID | SLE_VAR_U16,
	SLE_STRINGBUF   = SLE_FILE_STRING   | SLE_VAR_STRB,
	SLE_STRINGQUOTE = SLE_FILE_STRING   | SLE_VAR_STRQ,

	/* Shortcut values */
	SLE_UINT = SLE_UINT32,
	SLE_INT  = SLE_INT32,
	SLE_STRB = SLE_STRINGBUF,
	SLE_STRQ = SLE_STRINGQUOTE,
};

typedef uint32 VarType;

enum SaveLoadTypes {
	SL_VAR       = 0,
	SL_REF       = 1,
	SL_ARR       = 2,
	SL_STR       = 3,
	// non-normal save-load types
	SL_WRITEBYTE = 8,
	SL_INCLUDE   = 9,
	SL_END       = 15
};

typedef byte SaveLoadType;
typedef uint16 OffSetType;

/** SaveLoad type struct. Do NOT use this directly but use the SLE_ macros defined just below! */
typedef struct SaveLoad {
	SaveLoadType cmd;     /// the action to take with the saved/loaded type, All types need different action
	VarType conv;         /// type of the variable to be saved, int
	uint16 length;        /// (conditional) length of the variable (eg. arrays) (max array size is 65536 elements)
	uint16 version_from;  /// save/load the variable starting from this savegame version
	uint16 version_to;    /// save/load the variable until this savegame version
	/* NOTE: This element either denotes the address of the variable for a global
	 * variable, or the offset within a struct which is then bound to a variable
	 * during runtime. Decision on which one to use is controlled by the function
	 * that is called to save it. address: SlGlobList, offset: SlObject */
	union {
		void *address;      /// address of variable
		OffSetType offset;  /// offset of variable in the struct (max offset is 65536)
	} s;
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
#define SLE_END() {SL_END, 0, 0, 0, 0, (void*)0}

/* Simple variables, references (pointers) and arrays, but for global variables */
#define SLEG_GENERAL(cmd, variable, type, length, from, to) {cmd, type, length, from, to, &variable}

#define SLEG_CONDVAR(variable, type, from, to) SLEG_GENERAL(SL_VAR, variable, type, 0, from, to)
#define SLEG_CONDREF(variable, type, from, to) SLEG_GENERAL(SL_REF, variable, type, 0, from, to)
#define SLEG_CONDARR(variable, type, length, from, to) SLEG_GENERAL(SL_ARR, variable, type, length, from, to)
#define SLEG_CONDSTR(variable, type, length, from, to) SLEG_GENERAL(SL_STR, variable, type, length, from, to)

#define SLEG_VAR(variable, type) SLEG_CONDVAR(variable, type, 0, SL_MAX_VERSION)
#define SLEG_REF(variable, type) SLEG_CONDREF(variable, type, 0, SL_MAX_VERSION)
#define SLEG_ARR(variable, type) SLEG_CONDARR(variable, type, lengthof(variable), SL_MAX_VERSION)
#define SLEG_STR(variable, type) SLEG_CONDSTR(variable, type, lengthof(variable), SL_MAX_VERSION)

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
