#include "stdafx.h"
#include "ttd.h"
#include "vehicle.h"
#include "station.h"
#include "town.h"
#include "player.h"
#include "saveload.h"
#include <setjmp.h>

enum {
	SAVEGAME_MAJOR_VERSION = 4,
	SAVEGAME_MINOR_VERSION = 1,

	SAVEGAME_LOADABLE_VERSION = (SAVEGAME_MAJOR_VERSION << 8) + SAVEGAME_MINOR_VERSION
};

/******************************************************/
/******************************************************/
/******************************************************/

typedef void WriterProc(uint len);
typedef uint ReaderProc();

typedef uint ReferenceToIntProc(void *v, uint t);
typedef void *IntToReferenceProc(uint r, uint t);

typedef struct {
	bool save;
	byte need_length;
	byte block_mode;
	bool error;
	byte version;

	int obj_len;
	int array_index, last_array_index;

	uint32 offs_base;

	WriterProc *write_bytes;
	ReaderProc *read_bytes;

	ReferenceToIntProc *ref_to_int_proc;
	IntToReferenceProc *int_to_ref_proc;

	const ChunkHandler * const * chs;
	const byte * const *includes;

	byte *bufp, *bufe;

	int tmp;

	// these 3 may be used by compressor/decompressors.
	byte *buf; // pointer and size to read/write, initialized by init
	uint bufsize;
	FILE *fh;

	void (*excpt_uninit)();
	const char *excpt_msg;
	jmp_buf excpt; // used to jump to "exception handler"
} SaverLoader;

enum NeedLengthValues { NL_NONE = 0,NL_WANTLENGTH = 1,NL_CALCLENGTH = 2};

SaverLoader _sl;

// fill the input buffer
static void SlReadFill()
{
	uint len = _sl.read_bytes();
	assert(len != 0);

	_sl.bufp = _sl.buf;
	_sl.bufe = _sl.buf + len;
	_sl.offs_base += len;
}

static uint32 SlGetOffs()
{
	return _sl.offs_base - (_sl.bufe - _sl.bufp);
}

// flush the output buffer
static void SlWriteFill()
{
	// flush current buffer?
	if (_sl.bufp != NULL) {
		uint len = _sl.bufp - _sl.buf;
		_sl.offs_base += len;
		if (len) _sl.write_bytes(len);
	}

	// setup next buffer
	_sl.bufp = _sl.buf;
	_sl.bufe = _sl.buf + _sl.bufsize;
}

// error handler, calls longjmp to simulate an exception.
static void NORETURN SlError(const char *msg)
{
	_sl.excpt_msg = msg;
	longjmp(_sl.excpt, 0);
}

int SlReadByte()
{
	if (_sl.bufp == _sl.bufe) SlReadFill();
	return *_sl.bufp++;
}

void SlWriteByte(byte v)
{
	if (_sl.bufp == _sl.bufe) SlWriteFill();
	*_sl.bufp++ = v;
}

static int SlReadUint16()
{
	int x = SlReadByte() << 8;
	return x | SlReadByte();
}

static uint32 SlReadUint32()
{
	uint32 x = SlReadUint16() << 16;
	return x | SlReadUint16();
}

static uint64 SlReadUint64()
{
	uint32 x = SlReadUint32();
	uint32 y = SlReadUint32();
	return (uint64)x << 32 | y;
}

static void SlWriteUint16(uint16 v)
{
	SlWriteByte((byte)(v >> 8));
	SlWriteByte((byte)v);
}

static void SlWriteUint32(uint32 v)
{
	SlWriteUint16((uint16)(v >> 16));
	SlWriteUint16((uint16)v);
}

static void SlWriteUint64(uint64 x)
{
	SlWriteUint32((uint32)(x >> 32));
	SlWriteUint32((uint32)x);
}

static int SlReadSimpleGamma()
{
	int x = SlReadByte();
	if (x & 0x80)
		x = ((x&0x7F) << 8) + SlReadByte();
	return x;
}

static void SlWriteSimpleGamma(uint i)
{
	assert(i < (1 << 14));
	if (i >= 0x80) {
		SlWriteByte((byte)(0x80|(i >> 8)));
		SlWriteByte((byte)i);
	} else {
		SlWriteByte(i);
	}
}

static uint SlGetGammaLength(uint i) {
	return (i>=0x80) ? 2 : 1;
}

inline int SlReadSparseIndex()
{
	return SlReadSimpleGamma();
}

inline void SlWriteSparseIndex(uint index)
{
	SlWriteSimpleGamma(index);
}

inline int SlReadArrayLength()
{
	return SlReadSimpleGamma();
}

inline void SlWriteArrayLength(uint length)
{
	SlWriteSimpleGamma(length);
}

void SlSetArrayIndex(uint index)
{
	_sl.need_length = NL_WANTLENGTH;
	_sl.array_index = index;
}

int SlIterateArray()
{
	int ind;
	static uint32 next_offs;

	// Must be at end of current block.
	assert(next_offs == 0 || SlGetOffs() == next_offs);

	while(true) {
		uint len = SlReadArrayLength();
		if (len == 0) {
			next_offs = 0;
			return -1;
		}

		_sl.obj_len = --len;
		next_offs = SlGetOffs() + len;

		switch(_sl.block_mode) {
		case CH_SPARSE_ARRAY:	ind = SlReadSparseIndex(); break;
		case CH_ARRAY:        ind = _sl.array_index++; break;
		default:
			DEBUG(misc, 0) ("SlIterateArray: error\n");
			return -1; // error
		}

		if (len != 0)
			return ind;
	}
}

// Sets the length of either a RIFF object or the number of items in an array.
void SlSetLength(uint length)
{
	switch(_sl.need_length) {
	case NL_WANTLENGTH:
		_sl.need_length = NL_NONE;
		switch(_sl.block_mode) {
		case CH_RIFF:
			// Really simple to write a RIFF length :)
			SlWriteUint32(length);
		break;
		case CH_ARRAY:
			assert(_sl.array_index >= _sl.last_array_index);
			while (++_sl.last_array_index <= _sl.array_index)
				SlWriteArrayLength(1);
			SlWriteArrayLength(length + 1);
			break;
		case CH_SPARSE_ARRAY:
			SlWriteArrayLength(length + 1 + SlGetGammaLength(_sl.array_index)); // Also include length of sparse index.
			SlWriteSparseIndex(_sl.array_index);
			break;
		default: NOT_REACHED();
		}
		break;
	case NL_CALCLENGTH:
		_sl.obj_len += length;
		break;
	}
}

static void SlCopyBytes(void *ptr, size_t length)
{
	byte *p = (byte*)ptr;

	if (_sl.save) {
		while(length) {
			SlWriteByte(*p++);
			length--;
		}
	} else {
		while(length) {
			// INLINED SlReadByte
#if !defined(_DEBUG)
			if (_sl.bufp == _sl.bufe) SlReadFill();
			*p++ = *_sl.bufp++;
#else
			*p++ = SlReadByte();
#endif
			length--;
		}
	}
}

void SlSkipBytes(size_t length)
{
	while (length) {
		SlReadByte();
		length--;
	}
}

uint SlGetFieldLength()
{
	return _sl.obj_len;
}


static void SlSaveLoadConv(void *ptr, uint conv)
{
	int64 x = 0;

	if (_sl.save) {
		// Read a value from the struct. These ARE endian safe.
		switch((conv >> 4)&0xf) {
		case SLE_VAR_I8>>4: x = *(int8*)ptr; break;
		case SLE_VAR_U8>>4: x = *(byte*)ptr; break;
		case SLE_VAR_I16>>4: x = *(int16*)ptr; break;
		case SLE_VAR_U16>>4: x = *(uint16*)ptr; break;
		case SLE_VAR_I32>>4: x = *(int32*)ptr; break;
		case SLE_VAR_U32>>4: x = *(uint32*)ptr; break;
		case SLE_VAR_I64>>4: x = *(int64*)ptr; break;
		case SLE_VAR_U64>>4: x = *(uint64*)ptr; break;
		case SLE_VAR_NULL>>4: x = 0; break;
		default:
			NOT_REACHED();
		}

		// Write it to the file
		switch(conv & 0xF) {
		case SLE_FILE_I8: assert(x >= -128 && x <= 127); SlWriteByte(x);break;
		case SLE_FILE_U8:	assert(x >= 0 && x <= 255); SlWriteByte(x);break;
		case SLE_FILE_I16:assert(x >= -32768 && x <= 32767); SlWriteUint16(x);break;
		case SLE_FILE_STRINGID:
		case SLE_FILE_U16:assert(x >= 0 && x <= 65535); SlWriteUint16(x);break;
		case SLE_FILE_I32:
		case SLE_FILE_U32:SlWriteUint32((uint32)x);break;
		case SLE_FILE_I64:
		case SLE_FILE_U64:SlWriteUint64(x);break;
		default:
			assert(0);
			NOT_REACHED();
		}
	} else {

		// Read a value from the file
		switch(conv & 0xF) {
		case SLE_FILE_I8: x = (int8)SlReadByte();	break;
		case SLE_FILE_U8: x = (byte)SlReadByte(); break;
		case SLE_FILE_I16: x = (int16)SlReadUint16();	break;
		case SLE_FILE_U16: x = (uint16)SlReadUint16(); break;
		case SLE_FILE_I32: x = (int32)SlReadUint32();	break;
		case SLE_FILE_U32: x = (uint32)SlReadUint32(); break;
		case SLE_FILE_I64: x = (int64)SlReadUint64();	break;
		case SLE_FILE_U64: x = (uint64)SlReadUint64(); break;
		case SLE_FILE_STRINGID: x = RemapOldStringID((uint16)SlReadUint16()); break;
		default:
			assert(0);
			NOT_REACHED();
		}

		// Write it to the struct, these ARE endian safe.
		switch((conv >> 4)&0xf) {
		case SLE_VAR_I8>>4: *(int8*)ptr = x; break;
		case SLE_VAR_U8>>4: *(byte*)ptr = x; break;
		case SLE_VAR_I16>>4: *(int16*)ptr = x; break;
		case SLE_VAR_U16>>4: *(uint16*)ptr = x; break;
		case SLE_VAR_I32>>4: *(int32*)ptr = x; break;
		case SLE_VAR_U32>>4: *(uint32*)ptr = x; break;
		case SLE_VAR_I64>>4: *(int64*)ptr = x; break;
		case SLE_VAR_U64>>4: *(uint64*)ptr = x; break;
		case SLE_VAR_NULL: break;
		default:
			NOT_REACHED();
		}
	}
}

static const byte _conv_lengths[] = {1,1,2,2,4,4,8,8,2};

static uint SlCalcConvLen(uint conv, void *p)
{
	return _conv_lengths[conv & 0xF];
}

static uint SlCalcArrayLen(void *array, uint length, uint conv)
{
	return _conv_lengths[conv & 0xF] * length;
}

static const byte _conv_mem_size[9] = {1,1,2,2,4,4,8,8,0};
void SlArray(void *array, uint length, uint conv)
{
	// Automatically calculate the length?
	if (_sl.need_length != NL_NONE) {
		SlSetLength(SlCalcArrayLen(array, length, conv));
		// Determine length only?
		if (_sl.need_length == NL_CALCLENGTH)
			return;
	}

	// handle buggy stuff
	if (!_sl.save && _sl.version == 0) {
		if (conv == SLE_INT16 || conv == SLE_UINT16 || conv == SLE_STRINGID) {
			length *= 2;
			conv = SLE_INT8;
		} else if (conv == SLE_INT32 || conv == SLE_UINT32) {
			length *= 4;
			conv = SLE_INT8;
		}
	}

	// Optimized cases when input equals output.
	switch(conv) {
	case SLE_INT8:
	case SLE_UINT8:SlCopyBytes(array, length);break;
	default: {
		// Default "slow" case.
		byte *a = (byte*)array;
		while (length) {
			SlSaveLoadConv(a, conv);
			a += _conv_mem_size[(conv >> 4)&0xf];
			length--;
		}
		}
	}
}

// Calculate the size of an object.
static size_t SlCalcObjLength(void *object, const void *desc)
{
	size_t length = 0;
	uint cmd,conv;
	const byte *d = (const byte*)desc;

	// Need to determine the length and write a length tag.
	while (true) {
		cmd = (d[0] >> 4);
		if (cmd < 8) {
			conv = d[2];
			d += 3;
			if (cmd&4) {
				d += 2;
				// check if the field is of the right version
				if (_sl.version < d[-2] || _sl.version > d[-1]) {
					if ((cmd & 3) == 2) d++;
					continue;
				}
			}

			switch(cmd&3) {
			// Normal variable
			case 0: length += SlCalcConvLen(conv, NULL);break;
			// Reference
			case 1: length += 2; break;
			// Array
			case 2:	length += SlCalcArrayLen(NULL, *d++, conv); break;
			default:NOT_REACHED();
			}
		} else if (cmd == 8) {
			length++;
			d += 4;
		} else if (cmd == 9) {
			length += SlCalcObjLength(NULL, _sl.includes[d[2]]);
			d += 3;
		} else if (cmd == 15)
			break;
		else
			assert(0);
	}
	return length;
}

void SlObject(void *object, const void *desc)
{
	const byte *d = (const byte*)desc;
	void *ptr;
	uint cmd,conv;

	// Automatically calculate the length?
	if (_sl.need_length != NL_NONE) {
		SlSetLength(SlCalcObjLength(object, d));
		if (_sl.need_length == NL_CALCLENGTH)
			return;
	}

	while (true) {
		// Currently it only supports up to 4096 byte big objects
		ptr = (byte*)object + (d[0] & 0xF) + (d[1] << 4);

		cmd = d[0] >> 4;

		if (cmd < 8) {
			conv = d[2];
			d += 3;

			if (cmd&4) {
				d += 2;
				// check if the field is of the right version
				if (_sl.version < d[-2] || _sl.version > d[-1]) {
					if ((cmd & 3) == 2) d++;
					continue;
				}
			}

			switch(cmd&3) {
			// Normal variable
			case 0:	SlSaveLoadConv(ptr, conv); break;
			// Reference
			case 1:
				if (_sl.save) {
					SlWriteUint16(_sl.ref_to_int_proc(*(void**)ptr, conv));
				} else {
					*(void**)ptr = _sl.int_to_ref_proc(SlReadUint16(), conv);
				}
				break;
			// Array
			case 2:	SlArray(ptr, *d++, conv); break;
			default:NOT_REACHED();
			}

		// Write byte.
		} else if (cmd == 8) {
			if (_sl.save) {
				SlWriteByte(d[3]);
			} else {
				*(byte*)ptr = d[2];
			}
			d += 4;

		// Include
		} else if (cmd == 9) {
			SlObject(ptr, _sl.includes[d[2]]);
			d += 3;
		}	else if (cmd == 15)
			break;
		else
			assert(0);
	}
}


static size_t SlCalcGlobListLength(const SaveLoadGlobVarList *desc)
{
	size_t length = 0;

	while (desc->address) {
		if(_sl.version >= desc->from_version && _sl.version <= desc->to_version)
			length += SlCalcConvLen(desc->conv, NULL);
		desc++;
	}
	return length;
}

// Save/Load a list of global variables
void SlGlobList(const SaveLoadGlobVarList *desc)
{
	if (_sl.need_length != NL_NONE) {
		SlSetLength(SlCalcGlobListLength(desc));
		if (_sl.need_length == NL_CALCLENGTH)
			return;
	}
	while (true) {
		void *ptr = desc->address;
		if (ptr == NULL)
			break;
		if(_sl.version >= desc->from_version && _sl.version <= desc->to_version)
			SlSaveLoadConv(ptr, desc->conv);
		desc++;
	}
}


void SlAutolength(AutolengthProc *proc, void *arg)
{
	uint32 offs;

	assert(_sl.save);

	// Tell it to calculate the length
	_sl.need_length = NL_CALCLENGTH;
	_sl.obj_len = 0;
	proc(arg);

	// Setup length
	_sl.need_length = NL_WANTLENGTH;
	SlSetLength(_sl.obj_len);

	offs = SlGetOffs() + _sl.obj_len;

	// And write the stuff
	proc(arg);

	assert(offs == SlGetOffs());
}

static void SlLoadChunk(const ChunkHandler *ch)
{
	byte m = SlReadByte();
	size_t len;
	uint32 endoffs;

	_sl.block_mode = m;
	_sl.obj_len = 0;

	switch(m) {
	case CH_ARRAY:
		_sl.array_index = 0;
		ch->load_proc();
		break;

	case CH_SPARSE_ARRAY:
		ch->load_proc();
		break;

	case CH_RIFF:
		// Read length
		len = SlReadByte() << 16;
		len += SlReadUint16();
		_sl.obj_len = len;
		endoffs = SlGetOffs() + len;
		ch->load_proc();
		assert(SlGetOffs() == endoffs);
		break;
	default:
		assert(0);
	}
}

static ChunkSaveLoadProc *_tmp_proc_1;

static void SlStubSaveProc2(void *arg)
{
	_tmp_proc_1();
}

static void SlStubSaveProc()
{
	SlAutolength(SlStubSaveProc2, NULL);
}

static void SlSaveChunk(const ChunkHandler *ch)
{
	ChunkSaveLoadProc *proc;

	SlWriteUint32(ch->id);

	proc = ch->save_proc;
	if (ch->flags & CH_AUTO_LENGTH) {
		// Need to calculate the length. Solve that by calling SlAutoLength in the save_proc.
		_tmp_proc_1 = proc;
		proc = SlStubSaveProc;
	}

	_sl.block_mode = ch->flags & CH_TYPE_MASK;
	switch(ch->flags & CH_TYPE_MASK) {
	case CH_RIFF:
		_sl.need_length = NL_WANTLENGTH;
		proc();
		break;
	case CH_ARRAY:
		_sl.last_array_index = 0;
		SlWriteByte(CH_ARRAY);
		proc();
		SlWriteArrayLength(0); // Terminate arrays
		break;
	case CH_SPARSE_ARRAY:
		SlWriteByte(CH_SPARSE_ARRAY);
		proc();
		SlWriteArrayLength(0); // Terminate arrays
		break;
	default:
		NOT_REACHED();
	}
}

static void SlSaveChunks()
{
	const ChunkHandler *ch;
	const ChunkHandler * const * chsc;
	uint p;

	for(p=0; p!=CH_NUM_PRI_LEVELS; p++) {
		for(chsc=_sl.chs;(ch=*chsc++) != NULL;) {
			while(true) {
				if (((ch->flags >> CH_PRI_SHL) & (CH_NUM_PRI_LEVELS - 1)) == p)
					SlSaveChunk(ch);
				if (ch->flags & CH_LAST)
					break;
				ch++;
			}
		}
	}

	// Terminator
	SlWriteUint32(0);
}

static const ChunkHandler *SlFindChunkHandler(uint32 id)
{
	const ChunkHandler *ch;
	const ChunkHandler * const * chsc;
	for(chsc=_sl.chs;(ch=*chsc++) != NULL;) {
		while(true) {
			if (ch->id == id)
				return ch;
			if (ch->flags & CH_LAST)
				break;
			ch++;
		}
	}
	return NULL;
}

static void SlLoadChunks()
{
	uint32 id;
	const ChunkHandler *ch;

	while(true) {
		id = SlReadUint32();
		if (id == 0)
			return;
#if 0
		printf("Loading chunk %c%c%c%c\n", id >> 24, id>>16, id>>8,id);
#endif
		ch = SlFindChunkHandler(id);
		if (ch == NULL) SlError("found unknown tag in savegame (sync error)");
		SlLoadChunk(ch);
	}
}

//*******************************************
//********** START OF LZO CODE **************
//*******************************************
#define LZO_SIZE 8192

int CDECL lzo1x_1_compress( const byte *src, uint src_len,byte *dst, uint *dst_len,void *wrkmem );
uint32 CDECL lzo_adler32(uint32 adler, const byte *buf, uint len);
int CDECL lzo1x_decompress( const byte *src, uint  src_len,byte *dst, uint *dst_len,void *wrkmem /* NOT USED */ );

static uint ReadLZO()
{
	byte out[LZO_SIZE + LZO_SIZE / 64 + 16 + 3 + 8];
	uint32 tmp[2];
	uint32 size;
	uint len;

	// Read header
	if (fread(tmp, sizeof(tmp), 1, _sl.fh) != 1) SlError("file read failed");

	// Check if size is bad
	((uint32*)out)[0] = size = tmp[1];

	if (_sl.version != 0) {
		tmp[0] = TO_BE32(tmp[0]);
		size = TO_BE32(size);
	}

	if (size >= sizeof(out)) SlError("inconsistent size");

	// Read block
	if (fread(out + sizeof(uint32), size, 1, _sl.fh) != 1) SlError("file read failed");

	// Verify checksum
	if (tmp[0] != lzo_adler32(0, out, size + sizeof(uint32))) SlError("bad checksum");

	// Decompress
	lzo1x_decompress(out + sizeof(uint32)*1, size, _sl.buf, &len, NULL);
	return len;
}

// p contains the pointer to the buffer, len contains the pointer to the length.
// len bytes will be written, p and l will be updated to reflect the next buffer.
static void WriteLZO(uint size)
{
	byte out[LZO_SIZE + LZO_SIZE / 64 + 16 + 3 + 8];
	byte wrkmem[sizeof(byte*)*4096];
	uint outlen;

	lzo1x_1_compress(_sl.buf, size, out + sizeof(uint32)*2, &outlen, wrkmem);
	((uint32*)out)[1] = TO_BE32(outlen);
	((uint32*)out)[0] = TO_BE32(lzo_adler32(0, out + sizeof(uint32), outlen + sizeof(uint32)));
	if (fwrite(out, outlen + sizeof(uint32)*2, 1, _sl.fh) != 1) SlError("file write failed");
}

static bool InitLZO() {
	_sl.bufsize = LZO_SIZE;
	_sl.buf = (byte*)malloc(LZO_SIZE);
	return true;
}

static void UninitLZO() {
	free(_sl.buf);
}

//*******************************************
//******** START OF NOCOMP CODE *************
//*******************************************
static uint ReadNoComp()
{
	return fread(_sl.buf, 1, LZO_SIZE, _sl.fh);
}

static void WriteNoComp(uint size)
{
	fwrite(_sl.buf, 1, size, _sl.fh);
}

static bool InitNoComp()
{
	_sl.bufsize = LZO_SIZE;
	_sl.buf = (byte*)malloc(LZO_SIZE);
	return true;
}

static void UninitNoComp()
{
	free(_sl.buf);
}

//********************************************
//********** START OF ZLIB CODE **************
//********************************************

#if defined(WITH_ZLIB)
#include <zlib.h>
static z_stream _z;

static bool InitReadZlib()
{
	memset(&_z, 0, sizeof(_z));
	if (inflateInit(&_z) != Z_OK) return false;

	_sl.bufsize = 4096;
	_sl.buf = (byte*)malloc(4096 + 4096); // also contains fread buffer
	return true;
}

static uint ReadZlib()
{
	int r;

	_z.next_out = _sl.buf;
	_z.avail_out = 4096;

	do {
		// read more bytes from the file?
		if (_z.avail_in == 0) {
			_z.avail_in = fread(_z.next_in = _sl.buf + 4096, 1, 4096, _sl.fh);
		}

		// inflate the data
		r = inflate(&_z, 0);
		if (r == Z_STREAM_END)
			break;

		if (r != Z_OK)
			SlError("inflate() failed");
	} while (_z.avail_out);

	return 4096 - _z.avail_out;
}

static void UninitReadZlib()
{
	inflateEnd(&_z);
	free(_sl.buf);
}

static bool InitWriteZlib()
{
	memset(&_z, 0, sizeof(_z));
	if (deflateInit(&_z, 6) != Z_OK) return false;

	_sl.bufsize = 4096;
	_sl.buf = (byte*)malloc(4096); // also contains fread buffer
	return true;
}

static void WriteZlibLoop(z_streamp z, byte *p, uint len, int mode)
{
	char buf[1024]; // output buffer
	int r;
	uint n;
	z->next_in = p;
	z->avail_in = len;
	do {
		z->next_out = buf;
		z->avail_out = sizeof(buf);
		r	= deflate(z, mode);
			// bytes were emitted?
		if ((n=sizeof(buf) - z->avail_out) != 0) {
			if (fwrite(buf, n, 1, _sl.fh) != 1) SlError("file write error");
		}
		if (r == Z_STREAM_END)
			break;
		if (r != Z_OK) SlError("zlib returned error code");
	} while (z->avail_in || !z->avail_out);
}

static void WriteZlib(uint len)
{
	WriteZlibLoop(&_z, _sl.buf, len, 0);
}

static void UninitWriteZlib()
{
	// flush any pending output.
	if (_sl.fh) WriteZlibLoop(&_z, NULL, 0, Z_FINISH);
	deflateEnd(&_z);
	free(_sl.buf);
}

#endif //WITH_ZLIB

//*******************************************
//************* END OF CODE *****************
//*******************************************

// these define the chunks
extern const ChunkHandler _misc_chunk_handlers[];
extern const ChunkHandler _player_chunk_handlers[];
extern const ChunkHandler _veh_chunk_handlers[];
extern const ChunkHandler _town_chunk_handlers[];
extern const ChunkHandler _sign_chunk_handlers[];
extern const ChunkHandler _station_chunk_handlers[];
extern const ChunkHandler _industry_chunk_handlers[];
extern const ChunkHandler _engine_chunk_handlers[];
extern const ChunkHandler _economy_chunk_handlers[];
extern const ChunkHandler _animated_tile_chunk_handlers[];

static const ChunkHandler * const _chunk_handlers[] = {
	_misc_chunk_handlers,
	_veh_chunk_handlers,
	_industry_chunk_handlers,
	_economy_chunk_handlers,
	_engine_chunk_handlers,
	_town_chunk_handlers,
	_sign_chunk_handlers,
	_station_chunk_handlers,
	_player_chunk_handlers,
	_animated_tile_chunk_handlers,
	NULL,
};

// used to include a vehicle desc in another desc.
extern const byte _common_veh_desc[];
static const byte * const _desc_includes[] = {
	_common_veh_desc
};

typedef struct {
	void *base;
	size_t size;
} ReferenceSetup;

// used to translate "pointers"
static const ReferenceSetup _ref_setup[] = {
	{_order_array,sizeof(_order_array[0])},
	{_vehicles,sizeof(_vehicles[0])},
	{_stations,sizeof(_stations[0])},
	{_towns,sizeof(_towns[0])},
};

static uint ReferenceToInt(void *v, uint t)
{
	if (v == NULL) return 0;
	return ((byte*)v - (byte*)_ref_setup[t].base) / _ref_setup[t].size + 1;
}

void *IntToReference(uint r, uint t)
{
	if (r == 0) return NULL;
	return (byte*)_ref_setup[t].base + (r-1) * _ref_setup[t].size;
}

typedef struct {
	const char *name;
	uint32 tag;

	bool (*init_read)();
	ReaderProc *reader;
	void (*uninit_read)();

	bool (*init_write)();
	WriterProc *writer;
	void (*uninit_write)();

} SaveLoadFormat;

static const SaveLoadFormat _saveload_formats[] = {
	{"lzo", TO_BE32X('OTTD'), InitLZO,ReadLZO, UninitLZO, InitLZO, WriteLZO, UninitLZO},
	{"none", TO_BE32X('OTTN'), InitNoComp,ReadNoComp, UninitNoComp, InitNoComp, WriteNoComp, UninitNoComp},
#if defined(WITH_ZLIB)
	{"zlib", TO_BE32X('OTTZ'), InitReadZlib,ReadZlib, UninitReadZlib, InitWriteZlib, WriteZlib, UninitWriteZlib},
#else
	{"zlib", TO_BE32X('OTTZ'), NULL,NULL,NULL,NULL,NULL,NULL}
#endif
};

static const SaveLoadFormat *GetSavegameFormat(const char *s)
{
	const SaveLoadFormat *def;
	int i;

	// find default savegame format
	def = endof(_saveload_formats) - 1;
	while (!def->init_write) def--;

	if (_savegame_format[0]) {
		for(i = 0; i!=lengthof(_saveload_formats); i++)
			if (_saveload_formats[i].init_write && !strcmp(s, _saveload_formats[i].name))
				return _saveload_formats + i;

		ShowInfoF("Savegame format '%s' is not available. Reverting to '%s'.", s, def->name);
	}
	return def;
}

// actual loader/saver function
extern void InitializeGame();
extern bool AfterLoadGame(uint version);
extern void BeforeSaveGame();
extern bool LoadOldSaveGame(const char *file);

//	Save or Load files SL_LOAD, SL_SAVE, SL_OLD_LOAD
int SaveOrLoad(const char *filename, int mode)
{
	uint32 hdr[2];
	const SaveLoadFormat *fmt;
  uint version;

	// old style load
	if (mode == SL_OLD_LOAD) {
		InitializeGame();
		if (!LoadOldSaveGame(filename)) return SL_REINIT;
		AfterLoadGame(0);
		return SL_OK;
	}

	_sl.fh = fopen(filename, mode?"wb":"rb");
	if (_sl.fh == NULL)
		return SL_ERROR;

	_sl.bufe = _sl.bufp = NULL;
	_sl.offs_base = 0;
	_sl.int_to_ref_proc = IntToReference;
	_sl.ref_to_int_proc = ReferenceToInt;
	_sl.save = mode;
	_sl.includes = _desc_includes;
	_sl.chs = _chunk_handlers;

	// setup setjmp error handler
	if (setjmp(_sl.excpt)) {
		// close file handle.
		fclose(_sl.fh); _sl.fh = NULL;

		// deinitialize compressor.
		_sl.excpt_uninit();

		// a saver/loader exception!!
		// reinitialize all variables to prevent crash!
		if (mode == SL_LOAD) {
			ShowInfoF("Load game failed: %s.", _sl.excpt_msg);
			return SL_REINIT;
		} else {
			ShowInfoF("Save game failed: %s.", _sl.excpt_msg);
			return SL_ERROR;
		}
	}

  // we first initialize here to avoid: "warning: variable `version' might
  // be clobbered by `longjmp' or `vfork'"
	version = 0;

	if (mode != SL_LOAD) {
		fmt = GetSavegameFormat(_savegame_format);

		_sl.write_bytes = fmt->writer;
		_sl.excpt_uninit = fmt->uninit_write;
		if (!fmt->init_write()) goto init_err;

		hdr[0] = fmt->tag;
		hdr[1] = TO_BE32((SAVEGAME_MAJOR_VERSION<<16) + (SAVEGAME_MINOR_VERSION << 8));
		if (fwrite(hdr, sizeof(hdr), 1, _sl.fh) != 1) SlError("file write failed");

		_sl.version = SAVEGAME_MAJOR_VERSION;

		BeforeSaveGame();
		SlSaveChunks();
		SlWriteFill(); // flush the save buffer
		fmt->uninit_write();

	} else {
		if (fread(hdr, sizeof(hdr), 1, _sl.fh) != 1) {
read_err:
			printf("Savegame is obsolete or invalid format.\n");
init_err:
			fclose(_sl.fh);
			return SL_ERROR;
		}

		// see if we have any loader for this type.
		for(fmt = _saveload_formats; ; fmt++) {
			if (fmt == endof(_saveload_formats)) {
				printf("Unknown savegame type, trying to load it as the buggy format.\n");
				rewind(_sl.fh);
				_sl.version = 0;
				version = 0;
				fmt = _saveload_formats + 0; // lzo
				break;
			}
			if (fmt->tag == hdr[0]) {
				// check version number
				version = TO_BE32(hdr[1]) >> 8;

				// incompatible version?
				if (version > SAVEGAME_LOADABLE_VERSION) goto read_err;
				_sl.version = (version>>8);
				break;
			}
		}

		_sl.read_bytes = fmt->reader;
		_sl.excpt_uninit = fmt->uninit_read;

		// loader for this savegame type is not implemented?
		if (fmt->init_read == NULL) {
			ShowInfoF("Loader for '%s' is not available.", fmt->name);
			fclose(_sl.fh);
			return SL_ERROR;
		}

		if (!fmt->init_read()) goto init_err;
		// Clear everything
		InitializeGame();
		SlLoadChunks();
		fmt->uninit_read();
	}

	fclose(_sl.fh);

	if (mode == SL_LOAD) {
		if (!AfterLoadGame(version))
			return SL_REINIT;
	}

	return SL_OK;
}

bool EmergencySave()
{
	SaveOrLoad("crash.sav", SL_SAVE);
	return true;
}

void DoExitSave()
{
	char buf[200];
	sprintf(buf, "%s%sexit.sav", _path.autosave_dir, PATHSEP);
	SaveOrLoad(buf, SL_SAVE);
}

// not used right now, but could be used if extensions of savegames are garbled
/*int GetSavegameType(char *file)
{
	const SaveLoadFormat *fmt;
	uint32 hdr;
	FILE *f;
	int mode = SL_OLD_LOAD;

	f = fopen(file, "rb");
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		printf("Savegame is obsolete or invalid format.\n");
		mode = SL_LOAD;	// don't try to get filename, just show name as it is written
	}
	else {
		// see if we have any loader for this type.
		for (fmt = _saveload_formats; fmt != endof(_saveload_formats); fmt++) {
			if (fmt->tag == hdr) {
				mode = SL_LOAD; // new type of savegame
				break;
			}
		}
	}

	fclose(f);
	return mode;
}*/
