#ifndef SAVELOAD_H
#define SAVELOAD_H

typedef void ChunkSaveLoadProc();
typedef void AutolengthProc(void *arg);

typedef struct SaveLoadGlobVarList {
	void *address;
	byte conv;
	byte from_version;
	byte to_version;
} SaveLoadGlobVarList;


typedef struct {
	uint32 id;
	ChunkSaveLoadProc *save_proc;
	ChunkSaveLoadProc *load_proc;
	uint32 flags;
} ChunkHandler;

typedef struct {
	byte null;
} NullStruct;

enum {
	REF_SCHEDULE = 0,
	REF_VEHICLE = 1,
	REF_STATION = 2,
	REF_TOWN = 3,
};


enum {
	INC_VEHICLE_COMMON = 0,
};


enum {
	CH_RIFF = 0,
	CH_ARRAY = 1,
	CH_SPARSE_ARRAY = 2,
	CH_TYPE_MASK = 3,
	CH_LAST = 8,
	CH_AUTO_LENGTH = 16,
	CH_PRI_0 = 0 << 4,
	CH_PRI_1 = 1 << 4,
	CH_PRI_2 = 2 << 4,
	CH_PRI_3 = 3 << 4,
	CH_PRI_SHL = 4,
	CH_NUM_PRI_LEVELS = 4,
};

enum {
	SLE_FILE_I8 = 0,
	SLE_FILE_U8 = 1,
	SLE_FILE_I16 = 2,
	SLE_FILE_U16 = 3,
	SLE_FILE_I32 = 4,
	SLE_FILE_U32 = 5,
	SLE_FILE_I64 = 6,
	SLE_FILE_U64 = 7,

	SLE_FILE_STRINGID = 8,
//	SLE_FILE_IVAR = 8,
//	SLE_FILE_UVAR = 9,

	SLE_VAR_I8 = 0 << 4,
	SLE_VAR_U8 = 1 << 4,
	SLE_VAR_I16 = 2 << 4,
	SLE_VAR_U16 = 3 << 4,
	SLE_VAR_I32 = 4 << 4,
	SLE_VAR_U32 = 5 << 4,
	SLE_VAR_I64 = 6 << 4,
	SLE_VAR_U64 = 7 << 4,

	SLE_VAR_NULL = 8 << 4, // useful to write zeros in savegame.

	SLE_VAR_INT = SLE_VAR_I32,
	SLE_VAR_UINT = SLE_VAR_U32,

	SLE_INT8 = SLE_FILE_I8 | SLE_VAR_I8,
	SLE_UINT8 = SLE_FILE_U8 | SLE_VAR_U8,
	SLE_INT16 = SLE_FILE_I16 | SLE_VAR_I16,
	SLE_UINT16 = SLE_FILE_U16 | SLE_VAR_U16,
	SLE_INT32 = SLE_FILE_I32 | SLE_VAR_I32,
	SLE_UINT32 = SLE_FILE_U32 | SLE_VAR_U32,
	SLE_INT64 = SLE_FILE_I64 | SLE_VAR_I64,
	SLE_UINT64 = SLE_FILE_U64 | SLE_VAR_U64,

	SLE_STRINGID = SLE_FILE_STRINGID | SLE_VAR_U16,
};

#define SLE_VAR(t,i,c) 0 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c
#if _MSC_VER == 1200 /* XXX workaround for MSVC6 */
#define SLE_VAR2(t0,i0, t1, i1, c) 0 | ((offsetof(t0, i0) + offsetof(t1, i1)) & 0xF), (offsetof(t0, i0) + offsetof(t1, i1)) >> 4, c
#endif
#define SLE_REF(t,i,c) 0x10 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c
#define SLE_ARR(t,i,c,l) 0x20 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c, l
#define SLE_CONDVAR(t,i,c,from,to) 0x40 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c, from, to
#define SLE_CONDREF(t,i,c,from,to) 0x50 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c, from, to
#define SLE_CONDARR(t,i,c,l,from,to) 0x60 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c, from,to, l
#define SLE_WRITEBYTE(t,i,b,c) 0x80 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, b, c
#define SLE_INCLUDE(t,i,c) 0x90 | (offsetof(t,i) & 0xF), offsetof(t,i) >> 4, c


#define SLE_VARX(t,c) 0x00 | ((t) & 0xF), (t) >> 4, c
#define SLE_REFX(t,c) 0x10 | ((t) & 0xF), (t) >> 4, c
#define SLE_CONDVARX(t,c,from,to) 0x40 | ((t) & 0xF), (t) >> 4, c, from, to
#define SLE_CONDREFX(t,c,co) 0x50 | ((t) & 0xF), (t) >> 4, c, co
#define SLE_WRITEBYTEX(t,b) 0x80 | ((t) & 0xF), (t) >> 4, b
#define SLE_INCLUDEX(t,c) 0x90 | ((t) & 0xF), (t) >> 4, c

#define SLE_END() 0xF0


void SlSetArrayIndex(uint index);
int SlIterateArray();
void SlArray(void *array, uint length, uint conv);
void SlObject(void *object, const void *desc);
void SlAutolength(AutolengthProc *proc, void *arg);
uint SlGetFieldLength();
int SlReadByte();
void SlSetLength(uint length);
void SlWriteByte(byte v);
void SlGlobList(const SaveLoadGlobVarList *desc);

//int GetSavegameType(char *file);

#endif /* SAVELOAD_H */
