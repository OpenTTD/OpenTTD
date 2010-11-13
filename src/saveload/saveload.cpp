/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file saveload.cpp
 * All actions handling saving and loading goes on in this file. The general actions
 * are as follows for saving a game (loading is analogous):
 * <ol>
 * <li>initialize the writer by creating a temporary memory-buffer for it
 * <li>go through all to-be saved elements, each 'chunk' (ChunkHandler) prefixed by a label
 * <li>use their description array (SaveLoad) to know what elements to save and in what version
 *    of the game it was active (used when loading)
 * <li>write all data byte-by-byte to the temporary buffer so it is endian-safe
 * <li>when the buffer is full; flush it to the output (eg save to file) (_sl.buf, _sl.bufp, _sl.bufe)
 * <li>repeat this until everything is done, and flush any remaining output to file
 * </ol>
 */
#include "../stdafx.h"
#include "../debug.h"
#include "../station_base.h"
#include "../thread/thread.h"
#include "../town.h"
#include "../network/network.h"
#include "../window_func.h"
#include "../strings_func.h"
#include "../core/endian_func.hpp"
#include "../vehicle_base.h"
#include "../company_func.h"
#include "../date_func.h"
#include "../autoreplace_base.h"
#include "../roadstop_base.h"
#include "../statusbar_gui.h"
#include "../fileio_func.h"
#include "../gamelog.h"
#include "../string_func.h"
#include "../engine_base.h"
#include "../fios.h"

#include "table/strings.h"

#include "saveload_internal.h"

/*
 * Previous savegame versions, the trunk revision where they were
 * introduced and the released version that had that particular
 * savegame version.
 * Up to savegame version 18 there is a minor version as well.
 *
 *    1.0         0.1.x, 0.2.x
 *    2.0         0.3.0
 *    2.1         0.3.1, 0.3.2
 *    3.x         lost
 *    4.0     1
 *    4.1   122   0.3.3, 0.3.4
 *    4.2  1222   0.3.5
 *    4.3  1417
 *    4.4  1426
 *    5.0  1429
 *    5.1  1440
 *    5.2  1525   0.3.6
 *    6.0  1721
 *    6.1  1768
 *    7.0  1770
 *    8.0  1786
 *    9.0  1909
 *   10.0  2030
 *   11.0  2033
 *   11.1  2041
 *   12.1  2046
 *   13.1  2080   0.4.0, 0.4.0.1
 *   14.0  2441
 *   15.0  2499
 *   16.0  2817
 *   16.1  3155
 *   17.0  3212
 *   17.1  3218
 *   18    3227
 *   19    3396
 *   20    3403
 *   21    3472   0.4.x
 *   22    3726
 *   23    3915
 *   24    4150
 *   25    4259
 *   26    4466
 *   27    4757
 *   28    4987
 *   29    5070
 *   30    5946
 *   31    5999
 *   32    6001
 *   33    6440
 *   34    6455
 *   35    6602
 *   36    6624
 *   37    7182
 *   38    7195
 *   39    7269
 *   40    7326
 *   41    7348   0.5.x
 *   42    7573
 *   43    7642
 *   44    8144
 *   45    8501
 *   46    8705
 *   47    8735
 *   48    8935
 *   49    8969
 *   50    8973
 *   51    8978
 *   52    9066
 *   53    9316
 *   54    9613
 *   55    9638
 *   56    9667
 *   57    9691
 *   58    9762
 *   59    9779
 *   60    9874
 *   61    9892
 *   62    9905
 *   63    9956
 *   64   10006
 *   65   10210
 *   66   10211
 *   67   10236
 *   68   10266
 *   69   10319
 *   70   10541
 *   71   10567
 *   72   10601
 *   73   10903
 *   74   11030
 *   75   11107
 *   76   11139
 *   77   11172
 *   78   11176
 *   79   11188
 *   80   11228
 *   81   11244
 *   82   11410
 *   83   11589
 *   84   11822
 *   85   11874
 *   86   12042
 *   87   12129
 *   88   12134
 *   89   12160
 *   90   12293
 *   91   12347
 *   92   12381   0.6.x
 *   93   12648
 *   94   12816
 *   95   12924
 *   96   13226
 *   97   13256
 *   98   13375
 *   99   13838
 *  100   13952
 *  101   14233
 *  102   14332
 *  103   14598
 *  104   14735
 *  105   14803
 *  106   14919
 *  107   15027
 *  108   15045
 *  109   15075
 *  110   15148
 *  111   15190
 *  112   15290
 *  113   15340
 *  114   15601
 *  115   15695
 *  116   15893   0.7.x
 *  117   16037
 *  118   16129
 *  119   16242
 *  120   16439
 *  121   16694
 *  122   16855
 *  123   16909
 *  124   16993
 *  125   17113
 *  126   17433
 *  127   17439
 *  128   18281
 *  129   18292
 *  130   18404
 *  131   18481
 *  132   18522
 *  133   18674
 *  134   18703
 *  135   18719
 *  136   18764
 *  137   18912
 *  138   18942   1.0.x
 *  139   19346
 *  140   19382
 *  141   19799
 *  142   20003
 *  143   20048
 *  144   20334
 *  145   20376
 *  146   20446
 *  147   20621
 *  148   20659
 *  149   20832
 *  150   20857
 *  151   20918
 *  152   21171
 */
extern const uint16 SAVEGAME_VERSION = 152; ///< Current savegame version of OpenTTD.

SavegameType _savegame_type; ///< type of savegame we are loading

uint32 _ttdp_version;     ///< version of TTDP savegame (if applicable)
uint16 _sl_version;       ///< the major savegame version identifier
byte   _sl_minor_version; ///< the minor savegame version, DO NOT USE!
char _savegame_format[8]; ///< how to compress savegames
bool _do_autosave;        ///< are we doing an autosave at the moment?

typedef void WriterProc(size_t len);
typedef size_t ReaderProc();

/** What are we currently doing? */
enum SaveLoadAction {
	SLA_LOAD,        ///< loading
	SLA_SAVE,        ///< saving
	SLA_PTRS,        ///< fixing pointers
	SLA_NULL,        ///< null all pointers (on loading error)
	SLA_LOAD_CHECK,  ///< partial loading into #_load_check_data
};

enum NeedLength {
	NL_NONE = 0,       ///< not working in NeedLength mode
	NL_WANTLENGTH = 1, ///< writing length and data
	NL_CALCLENGTH = 2, ///< need to calculate the length
};

/** The saveload struct, containing reader-writer functions, bufffer, version, etc. */
struct SaveLoadParams {
	SaveLoadAction action;               ///< are we doing a save or a load atm.
	NeedLength need_length;              ///< working in NeedLength (Autolength) mode?
	byte block_mode;                     ///< ???
	bool error;                          ///< did an error occur or not

	size_t obj_len;                      ///< the length of the current object we are busy with
	int array_index, last_array_index;   ///< in the case of an array, the current and last positions

	size_t offs_base;                    ///< the offset in number of bytes since we started writing data (eg uncompressed savegame size)

	WriterProc *write_bytes;             ///< savegame writer function
	ReaderProc *read_bytes;              ///< savegame loader function

	/* When saving/loading savegames, they are always saved to a temporary memory-place
	 * to be flushed to file (save) or to final place (load) when full. */
	byte *bufp, *bufe;                   ///< bufp(ointer) gives the current position in the buffer bufe(nd) gives the end of the buffer

	/* these 3 may be used by compressor/decompressors. */
	byte *buf;                           ///< pointer to temporary memory to read/write, initialized by SaveLoadFormat->initread/write
	byte *buf_ori;                       ///< pointer to the original memory location of buf, used to free it afterwards
	uint bufsize;                        ///< the size of the temporary memory *buf
	FILE *fh;                            ///< the file from which is read or written to

	void (*excpt_uninit)();              ///< the function to execute on any encountered error
	StringID error_str;                  ///< the translateable error message to show
	char *extra_msg;                     ///< the error message
};

/* these define the chunks */
extern const ChunkHandler _gamelog_chunk_handlers[];
extern const ChunkHandler _map_chunk_handlers[];
extern const ChunkHandler _misc_chunk_handlers[];
extern const ChunkHandler _name_chunk_handlers[];
extern const ChunkHandler _cheat_chunk_handlers[] ;
extern const ChunkHandler _setting_chunk_handlers[];
extern const ChunkHandler _company_chunk_handlers[];
extern const ChunkHandler _engine_chunk_handlers[];
extern const ChunkHandler _veh_chunk_handlers[];
extern const ChunkHandler _waypoint_chunk_handlers[];
extern const ChunkHandler _depot_chunk_handlers[];
extern const ChunkHandler _order_chunk_handlers[];
extern const ChunkHandler _town_chunk_handlers[];
extern const ChunkHandler _sign_chunk_handlers[];
extern const ChunkHandler _station_chunk_handlers[];
extern const ChunkHandler _industry_chunk_handlers[];
extern const ChunkHandler _economy_chunk_handlers[];
extern const ChunkHandler _subsidy_chunk_handlers[];
extern const ChunkHandler _ai_chunk_handlers[];
extern const ChunkHandler _animated_tile_chunk_handlers[];
extern const ChunkHandler _newgrf_chunk_handlers[];
extern const ChunkHandler _group_chunk_handlers[];
extern const ChunkHandler _cargopacket_chunk_handlers[];
extern const ChunkHandler _autoreplace_chunk_handlers[];
extern const ChunkHandler _labelmaps_chunk_handlers[];
extern const ChunkHandler _airport_chunk_handlers[];
extern const ChunkHandler _object_chunk_handlers[];

static const ChunkHandler * const _chunk_handlers[] = {
	_gamelog_chunk_handlers,
	_map_chunk_handlers,
	_misc_chunk_handlers,
	_name_chunk_handlers,
	_cheat_chunk_handlers,
	_setting_chunk_handlers,
	_veh_chunk_handlers,
	_waypoint_chunk_handlers,
	_depot_chunk_handlers,
	_order_chunk_handlers,
	_industry_chunk_handlers,
	_economy_chunk_handlers,
	_subsidy_chunk_handlers,
	_engine_chunk_handlers,
	_town_chunk_handlers,
	_sign_chunk_handlers,
	_station_chunk_handlers,
	_company_chunk_handlers,
	_ai_chunk_handlers,
	_animated_tile_chunk_handlers,
	_newgrf_chunk_handlers,
	_group_chunk_handlers,
	_cargopacket_chunk_handlers,
	_autoreplace_chunk_handlers,
	_labelmaps_chunk_handlers,
	_airport_chunk_handlers,
	_object_chunk_handlers,
	NULL,
};

/**
 * Iterate over all chunk handlers.
 * @param ch the chunk handler iterator
 */
#define FOR_ALL_CHUNK_HANDLERS(ch) \
	for (const ChunkHandler * const *chsc = _chunk_handlers; *chsc != NULL; chsc++) \
		for (const ChunkHandler *ch = *chsc; ch != NULL; ch = (ch->flags & CH_LAST) ? NULL : ch + 1)

static SaveLoadParams _sl;

/** Null all pointers (convert index -> NULL) */
static void SlNullPointers()
{
	_sl.action = SLA_NULL;

	DEBUG(sl, 1, "Nulling pointers");

	FOR_ALL_CHUNK_HANDLERS(ch) {
		if (ch->ptrs_proc != NULL) {
			DEBUG(sl, 2, "Nulling pointers for %c%c%c%c", ch->id >> 24, ch->id >> 16, ch->id >> 8, ch->id);
			ch->ptrs_proc();
		}
	}

	DEBUG(sl, 1, "All pointers nulled");

	assert(_sl.action == SLA_NULL);
}

/**
 * Error handler. Sets everything up to show an error message and to clean
 * up the mess of a partial savegame load.
 * @param string The translatable error message to show.
 * @param extra_msg An extra error message coming from one of the APIs.
 * @note This function does never return as it throws an exception to
 *       break out of all the saveload code.
 */
static void NORETURN SlError(StringID string, const char *extra_msg = NULL)
{
	/* Distinguish between loading into _load_check_data vs. normal save/load. */
	if (_sl.action == SLA_LOAD_CHECK) {
		_load_check_data.error = string;
		free(_load_check_data.error_data);
		_load_check_data.error_data = (extra_msg == NULL) ? NULL : strdup(extra_msg);
	} else {
		_sl.error_str = string;
		free(_sl.extra_msg);
		_sl.extra_msg = (extra_msg == NULL) ? NULL : strdup(extra_msg);
		/* We have to NULL all pointers here; we might be in a state where
		 * the pointers are actually filled with indices, which means that
		 * when we access them during cleaning the pool dereferences of
		 * those indices will be made with segmentation faults as result. */
	}
	if (_sl.action == SLA_LOAD || _sl.action == SLA_PTRS) SlNullPointers();
	throw std::exception();
}

/**
 * Error handler for corrupt savegames. Sets everything up to show the
 * error message and to clean up the mess of a partial savegame load.
 * @param msg Location the corruption has been spotted.
 * @note This function does never return as it throws an exception to
 *       break out of all the saveload code.
 */
void NORETURN SlErrorCorrupt(const char *msg)
{
	SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_SAVEGAME, msg);
}


typedef void (*AsyncSaveFinishProc)();                ///< Callback for when the savegame loading is finished.
static AsyncSaveFinishProc _async_save_finish = NULL; ///< Callback to call when the savegame loading is finished.
static ThreadObject *_save_thread;                    ///< The thread we're using to compress and write a savegame

/**
 * Called by save thread to tell we finished saving.
 * @param proc The callback to call when saving is done.
 */
static void SetAsyncSaveFinish(AsyncSaveFinishProc proc)
{
	if (_exit_game) return;
	while (_async_save_finish != NULL) CSleep(10);

	_async_save_finish = proc;
}

/**
 * Handle async save finishes.
 */
void ProcessAsyncSaveFinish()
{
	if (_async_save_finish == NULL) return;

	_async_save_finish();

	_async_save_finish = NULL;

	if (_save_thread != NULL) {
		_save_thread->Join();
		delete _save_thread;
		_save_thread = NULL;
	}
}

/**
 * Fill the input buffer by reading from the file with the given reader
 */
static void SlReadFill()
{
	size_t len = _sl.read_bytes();
	if (len == 0) SlErrorCorrupt("Unexpected end of chunk");

	_sl.bufp = _sl.buf;
	_sl.bufe = _sl.buf + len;
	_sl.offs_base += len;
}

static inline size_t SlGetOffs()
{
	return _sl.offs_base - (_sl.bufe - _sl.bufp);
}

/**
 * Flush the output buffer by writing to disk with the given reader.
 * If the buffer pointer has not yet been set up, set it up now. Usually
 * only called when the buffer is full, or there is no more data to be processed
 */
static void SlWriteFill()
{
	/* flush the buffer to disk (the writer) */
	if (_sl.bufp != NULL) {
		uint len = _sl.bufp - _sl.buf;
		_sl.offs_base += len;
		if (len) _sl.write_bytes(len);
	}

	/* All the data from the buffer has been written away, rewind to the beginning
	 * to start reading in more data */
	_sl.bufp = _sl.buf;
	_sl.bufe = _sl.buf + _sl.bufsize;
}

/**
 * Read in a single byte from file. If the temporary buffer is full,
 * flush it to its final destination
 * @return return the read byte from file
 */
static inline byte SlReadByteInternal()
{
	if (_sl.bufp == _sl.bufe) SlReadFill();
	return *_sl.bufp++;
}

/** Wrapper for SlReadByteInternal */
byte SlReadByte() {return SlReadByteInternal();}

/**
 * Write away a single byte from memory. If the temporary buffer is full,
 * flush it to its destination (file)
 * @param b the byte that is currently written
 */
static inline void SlWriteByteInternal(byte b)
{
	if (_sl.bufp == _sl.bufe) SlWriteFill();
	*_sl.bufp++ = b;
}

/** Wrapper for SlWriteByteInternal */
void SlWriteByte(byte b) {SlWriteByteInternal(b);}

static inline int SlReadUint16()
{
	int x = SlReadByte() << 8;
	return x | SlReadByte();
}

static inline uint32 SlReadUint32()
{
	uint32 x = SlReadUint16() << 16;
	return x | SlReadUint16();
}

static inline uint64 SlReadUint64()
{
	uint32 x = SlReadUint32();
	uint32 y = SlReadUint32();
	return (uint64)x << 32 | y;
}

static inline void SlWriteUint16(uint16 v)
{
	SlWriteByte(GB(v, 8, 8));
	SlWriteByte(GB(v, 0, 8));
}

static inline void SlWriteUint32(uint32 v)
{
	SlWriteUint16(GB(v, 16, 16));
	SlWriteUint16(GB(v,  0, 16));
}

static inline void SlWriteUint64(uint64 x)
{
	SlWriteUint32((uint32)(x >> 32));
	SlWriteUint32((uint32)x);
}

/**
 * Read in bytes from the file/data structure but don't do
 * anything with them, discarding them in effect
 * @param length The amount of bytes that is being treated this way
 */
static inline void SlSkipBytes(size_t length)
{
	for (; length != 0; length--) SlReadByte();
}

/**
 * Read in the header descriptor of an object or an array.
 * If the highest bit is set (7), then the index is bigger than 127
 * elements, so use the next byte to read in the real value.
 * The actual value is then both bytes added with the first shifted
 * 8 bits to the left, and dropping the highest bit (which only indicated a big index).
 * x = ((x & 0x7F) << 8) + SlReadByte();
 * @return Return the value of the index
 */
static uint SlReadSimpleGamma()
{
	uint i = SlReadByte();
	if (HasBit(i, 7)) {
		i &= ~0x80;
		if (HasBit(i, 6)) {
			i &= ~0x40;
			if (HasBit(i, 5)) {
				i &= ~0x20;
				if (HasBit(i, 4)) {
					SlErrorCorrupt("Unsupported gamma");
				}
				i = (i << 8) | SlReadByte();
			}
			i = (i << 8) | SlReadByte();
		}
		i = (i << 8) | SlReadByte();
	}
	return i;
}

/**
 * Write the header descriptor of an object or an array.
 * If the element is bigger than 127, use 2 bytes for saving
 * and use the highest byte of the first written one as a notice
 * that the length consists of 2 bytes, etc.. like this:
 * 0xxxxxxx
 * 10xxxxxx xxxxxxxx
 * 110xxxxx xxxxxxxx xxxxxxxx
 * 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
 * @param i Index being written
 */

static void SlWriteSimpleGamma(size_t i)
{
	if (i >= (1 << 7)) {
		if (i >= (1 << 14)) {
			if (i >= (1 << 21)) {
				assert(i < (1 << 28));
				SlWriteByte((byte)(0xE0 | (i >> 24)));
				SlWriteByte((byte)(i >> 16));
			} else {
				SlWriteByte((byte)(0xC0 | (i >> 16)));
			}
			SlWriteByte((byte)(i >> 8));
		} else {
			SlWriteByte((byte)(0x80 | (i >> 8)));
		}
	}
	SlWriteByte((byte)i);
}

/** Return how many bytes used to encode a gamma value */
static inline uint SlGetGammaLength(size_t i)
{
	return 1 + (i >= (1 << 7)) + (i >= (1 << 14)) + (i >= (1 << 21));
}

static inline uint SlReadSparseIndex()
{
	return SlReadSimpleGamma();
}

static inline void SlWriteSparseIndex(uint index)
{
	SlWriteSimpleGamma(index);
}

static inline uint SlReadArrayLength()
{
	return SlReadSimpleGamma();
}

static inline void SlWriteArrayLength(size_t length)
{
	SlWriteSimpleGamma(length);
}

static inline uint SlGetArrayLength(size_t length)
{
	return SlGetGammaLength(length);
}

/**
 * Return the size in bytes of a certain type of normal/atomic variable
 * as it appears in memory. See VarTypes
 * @param conv VarType type of variable that is used for calculating the size
 * @return Return the size of this type in bytes
 */
static inline uint SlCalcConvMemLen(VarType conv)
{
	static const byte conv_mem_size[] = {1, 1, 1, 2, 2, 4, 4, 8, 8, 0};
	byte length = GB(conv, 4, 4);

	switch (length << 4) {
		case SLE_VAR_STRB:
		case SLE_VAR_STRBQ:
		case SLE_VAR_STR:
		case SLE_VAR_STRQ:
			return SlReadArrayLength();

		default:
			assert(length < lengthof(conv_mem_size));
			return conv_mem_size[length];
	}
}

/**
 * Return the size in bytes of a certain type of normal/atomic variable
 * as it appears in a saved game. See VarTypes
 * @param conv VarType type of variable that is used for calculating the size
 * @return Return the size of this type in bytes
 */
static inline byte SlCalcConvFileLen(VarType conv)
{
	static const byte conv_file_size[] = {1, 1, 2, 2, 4, 4, 8, 8, 2};
	byte length = GB(conv, 0, 4);
	assert(length < lengthof(conv_file_size));
	return conv_file_size[length];
}

/** Return the size in bytes of a reference (pointer) */
static inline size_t SlCalcRefLen()
{
	return CheckSavegameVersion(69) ? 2 : 4;
}

void SlSetArrayIndex(uint index)
{
	_sl.need_length = NL_WANTLENGTH;
	_sl.array_index = index;
}

static size_t _next_offs;

/**
 * Iterate through the elements of an array and read the whole thing
 * @return The index of the object, or -1 if we have reached the end of current block
 */
int SlIterateArray()
{
	int index;

	/* After reading in the whole array inside the loop
	 * we must have read in all the data, so we must be at end of current block. */
	if (_next_offs != 0 && SlGetOffs() != _next_offs) SlErrorCorrupt("Invalid chunk size");

	while (true) {
		uint length = SlReadArrayLength();
		if (length == 0) {
			_next_offs = 0;
			return -1;
		}

		_sl.obj_len = --length;
		_next_offs = SlGetOffs() + length;

		switch (_sl.block_mode) {
			case CH_SPARSE_ARRAY: index = (int)SlReadSparseIndex(); break;
			case CH_ARRAY:        index = _sl.array_index++; break;
			default:
				DEBUG(sl, 0, "SlIterateArray error");
				return -1; // error
		}

		if (length != 0) return index;
	}
}

/**
 * Skip an array or sparse array
 */
void SlSkipArray()
{
	while (SlIterateArray() != -1) {
		SlSkipBytes(_next_offs - SlGetOffs());
	}
}

/**
 * Sets the length of either a RIFF object or the number of items in an array.
 * This lets us load an object or an array of arbitrary size
 * @param length The length of the sought object/array
 */
void SlSetLength(size_t length)
{
	assert(_sl.action == SLA_SAVE);

	switch (_sl.need_length) {
		case NL_WANTLENGTH:
			_sl.need_length = NL_NONE;
			switch (_sl.block_mode) {
				case CH_RIFF:
					/* Ugly encoding of >16M RIFF chunks
					 * The lower 24 bits are normal
					 * The uppermost 4 bits are bits 24:27 */
					assert(length < (1 << 28));
					SlWriteUint32((uint32)((length & 0xFFFFFF) | ((length >> 24) << 28)));
					break;
				case CH_ARRAY:
					assert(_sl.last_array_index <= _sl.array_index);
					while (++_sl.last_array_index <= _sl.array_index) {
						SlWriteArrayLength(1);
					}
					SlWriteArrayLength(length + 1);
					break;
				case CH_SPARSE_ARRAY:
					SlWriteArrayLength(length + 1 + SlGetArrayLength(_sl.array_index)); // Also include length of sparse index.
					SlWriteSparseIndex(_sl.array_index);
					break;
				default: NOT_REACHED();
			}
			break;

		case NL_CALCLENGTH:
			_sl.obj_len += (int)length;
			break;

		default: NOT_REACHED();
	}
}

/**
 * Save/Load bytes. These do not need to be converted to Little/Big Endian
 * so directly write them or read them to/from file
 * @param ptr The source or destination of the object being manipulated
 * @param length number of bytes this fast CopyBytes lasts
 */
static void SlCopyBytes(void *ptr, size_t length)
{
	byte *p = (byte *)ptr;

	switch (_sl.action) {
		case SLA_LOAD_CHECK:
		case SLA_LOAD:
			for (; length != 0; length--) *p++ = SlReadByteInternal();
			break;
		case SLA_SAVE:
			for (; length != 0; length--) SlWriteByteInternal(*p++);
			break;
		default: NOT_REACHED();
	}
}

/** Get the length of the current object */
size_t SlGetFieldLength()
{
	return _sl.obj_len;
}

/**
 * Return a signed-long version of the value of a setting
 * @param ptr pointer to the variable
 * @param conv type of variable, can be a non-clean
 * type, eg one with other flags because it is parsed
 * @return returns the value of the pointer-setting
 */
int64 ReadValue(const void *ptr, VarType conv)
{
	switch (GetVarMemType(conv)) {
		case SLE_VAR_BL:  return (*(bool *)ptr != 0);
		case SLE_VAR_I8:  return *(int8  *)ptr;
		case SLE_VAR_U8:  return *(byte  *)ptr;
		case SLE_VAR_I16: return *(int16 *)ptr;
		case SLE_VAR_U16: return *(uint16*)ptr;
		case SLE_VAR_I32: return *(int32 *)ptr;
		case SLE_VAR_U32: return *(uint32*)ptr;
		case SLE_VAR_I64: return *(int64 *)ptr;
		case SLE_VAR_U64: return *(uint64*)ptr;
		case SLE_VAR_NULL:return 0;
		default: NOT_REACHED();
	}
}

/**
 * Write the value of a setting
 * @param ptr pointer to the variable
 * @param conv type of variable, can be a non-clean type, eg
 *             with other flags. It is parsed upon read
 * @param val the new value being given to the variable
 */
void WriteValue(void *ptr, VarType conv, int64 val)
{
	switch (GetVarMemType(conv)) {
		case SLE_VAR_BL:  *(bool  *)ptr = (val != 0);  break;
		case SLE_VAR_I8:  *(int8  *)ptr = val; break;
		case SLE_VAR_U8:  *(byte  *)ptr = val; break;
		case SLE_VAR_I16: *(int16 *)ptr = val; break;
		case SLE_VAR_U16: *(uint16*)ptr = val; break;
		case SLE_VAR_I32: *(int32 *)ptr = val; break;
		case SLE_VAR_U32: *(uint32*)ptr = val; break;
		case SLE_VAR_I64: *(int64 *)ptr = val; break;
		case SLE_VAR_U64: *(uint64*)ptr = val; break;
		case SLE_VAR_NAME: *(char**)ptr = CopyFromOldName(val); break;
		case SLE_VAR_NULL: break;
		default: NOT_REACHED();
	}
}

/**
 * Handle all conversion and typechecking of variables here.
 * In the case of saving, read in the actual value from the struct
 * and then write them to file, endian safely. Loading a value
 * goes exactly the opposite way
 * @param ptr The object being filled/read
 * @param conv VarType type of the current element of the struct
 */
static void SlSaveLoadConv(void *ptr, VarType conv)
{
	switch (_sl.action) {
		case SLA_SAVE: {
			int64 x = ReadValue(ptr, conv);

			/* Write the value to the file and check if its value is in the desired range */
			switch (GetVarFileType(conv)) {
				case SLE_FILE_I8: assert(x >= -128 && x <= 127);     SlWriteByte(x);break;
				case SLE_FILE_U8: assert(x >= 0 && x <= 255);        SlWriteByte(x);break;
				case SLE_FILE_I16:assert(x >= -32768 && x <= 32767); SlWriteUint16(x);break;
				case SLE_FILE_STRINGID:
				case SLE_FILE_U16:assert(x >= 0 && x <= 65535);      SlWriteUint16(x);break;
				case SLE_FILE_I32:
				case SLE_FILE_U32:                                   SlWriteUint32((uint32)x);break;
				case SLE_FILE_I64:
				case SLE_FILE_U64:                                   SlWriteUint64(x);break;
				default: NOT_REACHED();
			}
			break;
		}
		case SLA_LOAD_CHECK:
		case SLA_LOAD: {
			int64 x;
			/* Read a value from the file */
			switch (GetVarFileType(conv)) {
				case SLE_FILE_I8:  x = (int8  )SlReadByte();   break;
				case SLE_FILE_U8:  x = (byte  )SlReadByte();   break;
				case SLE_FILE_I16: x = (int16 )SlReadUint16(); break;
				case SLE_FILE_U16: x = (uint16)SlReadUint16(); break;
				case SLE_FILE_I32: x = (int32 )SlReadUint32(); break;
				case SLE_FILE_U32: x = (uint32)SlReadUint32(); break;
				case SLE_FILE_I64: x = (int64 )SlReadUint64(); break;
				case SLE_FILE_U64: x = (uint64)SlReadUint64(); break;
				case SLE_FILE_STRINGID: x = RemapOldStringID((uint16)SlReadUint16()); break;
				default: NOT_REACHED();
			}

			/* Write The value to the struct. These ARE endian safe. */
			WriteValue(ptr, conv, x);
			break;
		}
		case SLA_PTRS: break;
		case SLA_NULL: break;
		default: NOT_REACHED();
	}
}

/**
 * Calculate the net length of a string. This is in almost all cases
 * just strlen(), but if the string is not properly terminated, we'll
 * resort to the maximum length of the buffer.
 * @param ptr pointer to the stringbuffer
 * @param length maximum length of the string (buffer). If -1 we don't care
 * about a maximum length, but take string length as it is.
 * @return return the net length of the string
 */
static inline size_t SlCalcNetStringLen(const char *ptr, size_t length)
{
	if (ptr == NULL) return 0;
	return min(strlen(ptr), length - 1);
}

/**
 * Calculate the gross length of the string that it
 * will occupy in the savegame. This includes the real length, returned
 * by SlCalcNetStringLen and the length that the index will occupy.
 * @param ptr pointer to the stringbuffer
 * @param length maximum length of the string (buffer size, etc.)
 * @param conv type of data been used
 * @return return the gross length of the string
 */
static inline size_t SlCalcStringLen(const void *ptr, size_t length, VarType conv)
{
	size_t len;
	const char *str;

	switch (GetVarMemType(conv)) {
		default: NOT_REACHED();
		case SLE_VAR_STR:
		case SLE_VAR_STRQ:
			str = *(const char**)ptr;
			len = SIZE_MAX;
			break;
		case SLE_VAR_STRB:
		case SLE_VAR_STRBQ:
			str = (const char*)ptr;
			len = length;
			break;
	}

	len = SlCalcNetStringLen(str, len);
	return len + SlGetArrayLength(len); // also include the length of the index
}

/**
 * Save/Load a string.
 * @param ptr the string being manipulated
 * @param length of the string (full length)
 * @param conv must be SLE_FILE_STRING
 */
static void SlString(void *ptr, size_t length, VarType conv)
{
	switch (_sl.action) {
		case SLA_SAVE: {
			size_t len;
			switch (GetVarMemType(conv)) {
				default: NOT_REACHED();
				case SLE_VAR_STRB:
				case SLE_VAR_STRBQ:
					len = SlCalcNetStringLen((char *)ptr, length);
					break;
				case SLE_VAR_STR:
				case SLE_VAR_STRQ:
					ptr = *(char **)ptr;
					len = SlCalcNetStringLen((char *)ptr, SIZE_MAX);
					break;
			}

			SlWriteArrayLength(len);
			SlCopyBytes(ptr, len);
			break;
		}
		case SLA_LOAD_CHECK:
		case SLA_LOAD: {
			size_t len = SlReadArrayLength();

			switch (GetVarMemType(conv)) {
				default: NOT_REACHED();
				case SLE_VAR_STRB:
				case SLE_VAR_STRBQ:
					if (len >= length) {
						DEBUG(sl, 1, "String length in savegame is bigger than buffer, truncating");
						SlCopyBytes(ptr, length);
						SlSkipBytes(len - length);
						len = length - 1;
					} else {
						SlCopyBytes(ptr, len);
					}
					break;
				case SLE_VAR_STR:
				case SLE_VAR_STRQ: // Malloc'd string, free previous incarnation, and allocate
					free(*(char **)ptr);
					if (len == 0) {
						*(char **)ptr = NULL;
					} else {
						*(char **)ptr = MallocT<char>(len + 1); // terminating '\0'
						ptr = *(char **)ptr;
						SlCopyBytes(ptr, len);
					}
					break;
			}

			((char *)ptr)[len] = '\0'; // properly terminate the string
			str_validate((char *)ptr, (char *)ptr + len);
			break;
		}
		case SLA_PTRS: break;
		case SLA_NULL: break;
		default: NOT_REACHED();
	}
}

/**
 * Return the size in bytes of a certain type of atomic array
 * @param length The length of the array counted in elements
 * @param conv VarType type of the variable that is used in calculating the size
 */
static inline size_t SlCalcArrayLen(size_t length, VarType conv)
{
	return SlCalcConvFileLen(conv) * length;
}

/**
 * Save/Load an array.
 * @param array The array being manipulated
 * @param length The length of the array in elements
 * @param conv VarType type of the atomic array (int, byte, uint64, etc.)
 */
void SlArray(void *array, size_t length, VarType conv)
{
	if (_sl.action == SLA_PTRS || _sl.action == SLA_NULL) return;

	/* Automatically calculate the length? */
	if (_sl.need_length != NL_NONE) {
		SlSetLength(SlCalcArrayLen(length, conv));
		/* Determine length only? */
		if (_sl.need_length == NL_CALCLENGTH) return;
	}

	/* NOTICE - handle some buggy stuff, in really old versions everything was saved
	 * as a byte-type. So detect this, and adjust array size accordingly */
	if (_sl.action != SLA_SAVE && _sl_version == 0) {
		/* all arrays except difficulty settings */
		if (conv == SLE_INT16 || conv == SLE_UINT16 || conv == SLE_STRINGID ||
				conv == SLE_INT32 || conv == SLE_UINT32) {
			SlCopyBytes(array, length * SlCalcConvFileLen(conv));
			return;
		}
		/* used for conversion of Money 32bit->64bit */
		if (conv == (SLE_FILE_I32 | SLE_VAR_I64)) {
			for (uint i = 0; i < length; i++) {
				((int64*)array)[i] = (int32)BSWAP32(SlReadUint32());
			}
			return;
		}
	}

	/* If the size of elements is 1 byte both in file and memory, no special
	 * conversion is needed, use specialized copy-copy function to speed up things */
	if (conv == SLE_INT8 || conv == SLE_UINT8) {
		SlCopyBytes(array, length);
	} else {
		byte *a = (byte*)array;
		byte mem_size = SlCalcConvMemLen(conv);

		for (; length != 0; length --) {
			SlSaveLoadConv(a, conv);
			a += mem_size; // get size
		}
	}
}


/**
 * Pointers cannot be saved to a savegame, so this functions gets
 * the index of the item, and if not available, it hussles with
 * pointers (looks really bad :()
 * Remember that a NULL item has value 0, and all
 * indices have +1, so vehicle 0 is saved as index 1.
 * @param obj The object that we want to get the index of
 * @param rt SLRefType type of the object the index is being sought of
 * @return Return the pointer converted to an index of the type pointed to
 */
static size_t ReferenceToInt(const void *obj, SLRefType rt)
{
	assert(_sl.action == SLA_SAVE);

	if (obj == NULL) return 0;

	switch (rt) {
		case REF_VEHICLE_OLD: // Old vehicles we save as new onces
		case REF_VEHICLE:   return ((const  Vehicle*)obj)->index + 1;
		case REF_STATION:   return ((const  Station*)obj)->index + 1;
		case REF_TOWN:      return ((const     Town*)obj)->index + 1;
		case REF_ORDER:     return ((const    Order*)obj)->index + 1;
		case REF_ROADSTOPS: return ((const RoadStop*)obj)->index + 1;
		case REF_ENGINE_RENEWS: return ((const EngineRenew*)obj)->index + 1;
		case REF_CARGO_PACKET:  return ((const CargoPacket*)obj)->index + 1;
		case REF_ORDERLIST:     return ((const   OrderList*)obj)->index + 1;
		default: NOT_REACHED();
	}
}

/**
 * Pointers cannot be loaded from a savegame, so this function
 * gets the index from the savegame and returns the appropiate
 * pointer from the already loaded base.
 * Remember that an index of 0 is a NULL pointer so all indices
 * are +1 so vehicle 0 is saved as 1.
 * @param index The index that is being converted to a pointer
 * @param rt SLRefType type of the object the pointer is sought of
 * @return Return the index converted to a pointer of any type
 */
static void *IntToReference(size_t index, SLRefType rt)
{
	assert_compile(sizeof(size_t) <= sizeof(void *));

	assert(_sl.action == SLA_PTRS);

	/* After version 4.3 REF_VEHICLE_OLD is saved as REF_VEHICLE,
	 * and should be loaded like that */
	if (rt == REF_VEHICLE_OLD && !CheckSavegameVersionOldStyle(4, 4)) {
		rt = REF_VEHICLE;
	}

	/* No need to look up NULL pointers, just return immediately */
	if (index == (rt == REF_VEHICLE_OLD ? 0xFFFF : 0)) return NULL;

	/* Correct index. Old vehicles were saved differently:
	 * invalid vehicle was 0xFFFF, now we use 0x0000 for everything invalid. */
	if (rt != REF_VEHICLE_OLD) index--;

	switch (rt) {
		case REF_ORDERLIST:
			if (OrderList::IsValidID(index)) return OrderList::Get(index);
			SlErrorCorrupt("Referencing invalid OrderList");

		case REF_ORDER:
			if (Order::IsValidID(index)) return Order::Get(index);
			/* in old versions, invalid order was used to mark end of order list */
			if (CheckSavegameVersionOldStyle(5, 2)) return NULL;
			SlErrorCorrupt("Referencing invalid Order");

		case REF_VEHICLE_OLD:
		case REF_VEHICLE:
			if (Vehicle::IsValidID(index)) return Vehicle::Get(index);
			SlErrorCorrupt("Referencing invalid Vehicle");

		case REF_STATION:
			if (Station::IsValidID(index)) return Station::Get(index);
			SlErrorCorrupt("Referencing invalid Station");

		case REF_TOWN:
			if (Town::IsValidID(index)) return Town::Get(index);
			SlErrorCorrupt("Referencing invalid Town");

		case REF_ROADSTOPS:
			if (RoadStop::IsValidID(index)) return RoadStop::Get(index);
			SlErrorCorrupt("Referencing invalid RoadStop");

		case REF_ENGINE_RENEWS:
			if (EngineRenew::IsValidID(index)) return EngineRenew::Get(index);
			SlErrorCorrupt("Referencing invalid EngineRenew");

		case REF_CARGO_PACKET:
			if (CargoPacket::IsValidID(index)) return CargoPacket::Get(index);
			SlErrorCorrupt("Referencing invalid CargoPacket");

		default: NOT_REACHED();
	}
}

/**
 * Return the size in bytes of a list
 * @param list The std::list to find the size of
 */
static inline size_t SlCalcListLen(const void *list)
{
	std::list<void *> *l = (std::list<void *> *) list;

	int type_size = CheckSavegameVersion(69) ? 2 : 4;
	/* Each entry is saved as type_size bytes, plus type_size bytes are used for the length
	 * of the list */
	return l->size() * type_size + type_size;
}


/**
 * Save/Load a list.
 * @param list The list being manipulated
 * @param conv SLRefType type of the list (Vehicle *, Station *, etc)
 */
static void SlList(void *list, SLRefType conv)
{
	/* Automatically calculate the length? */
	if (_sl.need_length != NL_NONE) {
		SlSetLength(SlCalcListLen(list));
		/* Determine length only? */
		if (_sl.need_length == NL_CALCLENGTH) return;
	}

	typedef std::list<void *> PtrList;
	PtrList *l = (PtrList *)list;

	switch (_sl.action) {
		case SLA_SAVE: {
			SlWriteUint32((uint32)l->size());

			PtrList::iterator iter;
			for (iter = l->begin(); iter != l->end(); ++iter) {
				void *ptr = *iter;
				SlWriteUint32((uint32)ReferenceToInt(ptr, conv));
			}
			break;
		}
		case SLA_LOAD_CHECK:
		case SLA_LOAD: {
			size_t length = CheckSavegameVersion(69) ? SlReadUint16() : SlReadUint32();

			/* Load each reference and push to the end of the list */
			for (size_t i = 0; i < length; i++) {
				size_t data = CheckSavegameVersion(69) ? SlReadUint16() : SlReadUint32();
				l->push_back((void *)data);
			}
			break;
		}
		case SLA_PTRS: {
			PtrList temp = *l;

			l->clear();
			PtrList::iterator iter;
			for (iter = temp.begin(); iter != temp.end(); ++iter) {
				void *ptr = IntToReference((size_t)*iter, conv);
				l->push_back(ptr);
			}
			break;
		}
		case SLA_NULL:
			l->clear();
			break;
		default: NOT_REACHED();
	}
}


/** Are we going to save this object or not? */
static inline bool SlIsObjectValidInSavegame(const SaveLoad *sld)
{
	if (_sl_version < sld->version_from || _sl_version > sld->version_to) return false;
	if (sld->conv & SLF_SAVE_NO) return false;

	return true;
}

/**
 * Are we going to load this variable when loading a savegame or not?
 * @note If the variable is skipped it is skipped in the savegame
 * bytestream itself as well, so there is no need to skip it somewhere else
 */
static inline bool SlSkipVariableOnLoad(const SaveLoad *sld)
{
	if ((sld->conv & SLF_NETWORK_NO) && _sl.action != SLA_SAVE && _networking && !_network_server) {
		SlSkipBytes(SlCalcConvMemLen(sld->conv) * sld->length);
		return true;
	}

	return false;
}

/**
 * Calculate the size of an object.
 * @param object to be measured
 * @param sld The SaveLoad description of the object so we know how to manipulate it
 * @return size of given objetc
 */
size_t SlCalcObjLength(const void *object, const SaveLoad *sld)
{
	size_t length = 0;

	/* Need to determine the length and write a length tag. */
	for (; sld->cmd != SL_END; sld++) {
		length += SlCalcObjMemberLength(object, sld);
	}
	return length;
}

size_t SlCalcObjMemberLength(const void *object, const SaveLoad *sld)
{
	assert(_sl.action == SLA_SAVE);

	switch (sld->cmd) {
		case SL_VAR:
		case SL_REF:
		case SL_ARR:
		case SL_STR:
		case SL_LST:
			/* CONDITIONAL saveload types depend on the savegame version */
			if (!SlIsObjectValidInSavegame(sld)) break;

			switch (sld->cmd) {
				case SL_VAR: return SlCalcConvFileLen(sld->conv);
				case SL_REF: return SlCalcRefLen();
				case SL_ARR: return SlCalcArrayLen(sld->length, sld->conv);
				case SL_STR: return SlCalcStringLen(GetVariableAddress(object, sld), sld->length, sld->conv);
				case SL_LST: return SlCalcListLen(GetVariableAddress(object, sld));
				default: NOT_REACHED();
			}
			break;
		case SL_WRITEBYTE: return 1; // a byte is logically of size 1
		case SL_VEH_INCLUDE: return SlCalcObjLength(object, GetVehicleDescription(VEH_END));
		case SL_ST_INCLUDE: return SlCalcObjLength(object, GetBaseStationDescription());
		default: NOT_REACHED();
	}
	return 0;
}


bool SlObjectMember(void *ptr, const SaveLoad *sld)
{
	VarType conv = GB(sld->conv, 0, 8);
	switch (sld->cmd) {
		case SL_VAR:
		case SL_REF:
		case SL_ARR:
		case SL_STR:
		case SL_LST:
			/* CONDITIONAL saveload types depend on the savegame version */
			if (!SlIsObjectValidInSavegame(sld)) return false;
			if (SlSkipVariableOnLoad(sld)) return false;

			switch (sld->cmd) {
				case SL_VAR: SlSaveLoadConv(ptr, conv); break;
				case SL_REF: // Reference variable, translate
					switch (_sl.action) {
						case SLA_SAVE:
							SlWriteUint32((uint32)ReferenceToInt(*(void **)ptr, (SLRefType)conv));
							break;
						case SLA_LOAD_CHECK:
						case SLA_LOAD:
							*(size_t *)ptr = CheckSavegameVersion(69) ? SlReadUint16() : SlReadUint32();
							break;
						case SLA_PTRS:
							*(void **)ptr = IntToReference(*(size_t *)ptr, (SLRefType)conv);
							break;
						case SLA_NULL:
							*(void **)ptr = NULL;
							break;
						default: NOT_REACHED();
					}
					break;
				case SL_ARR: SlArray(ptr, sld->length, conv); break;
				case SL_STR: SlString(ptr, sld->length, conv); break;
				case SL_LST: SlList(ptr, (SLRefType)conv); break;
				default: NOT_REACHED();
			}
			break;

		/* SL_WRITEBYTE translates a value of a variable to another one upon
		 * saving or loading.
		 * XXX - variable renaming abuse
		 * game_value: the value of the variable ingame is abused by sld->version_from
		 * file_value: the value of the variable in the savegame is abused by sld->version_to */
		case SL_WRITEBYTE:
			switch (_sl.action) {
				case SLA_SAVE: SlWriteByte(sld->version_to); break;
				case SLA_LOAD_CHECK:
				case SLA_LOAD: *(byte *)ptr = sld->version_from; break;
				case SLA_PTRS: break;
				case SLA_NULL: break;
				default: NOT_REACHED();
			}
			break;

		/* SL_VEH_INCLUDE loads common code for vehicles */
		case SL_VEH_INCLUDE:
			SlObject(ptr, GetVehicleDescription(VEH_END));
			break;

		case SL_ST_INCLUDE:
			SlObject(ptr, GetBaseStationDescription());
			break;

		default: NOT_REACHED();
	}
	return true;
}

/**
 * Main SaveLoad function.
 * @param object The object that is being saved or loaded
 * @param sld The SaveLoad description of the object so we know how to manipulate it
 */
void SlObject(void *object, const SaveLoad *sld)
{
	/* Automatically calculate the length? */
	if (_sl.need_length != NL_NONE) {
		SlSetLength(SlCalcObjLength(object, sld));
		if (_sl.need_length == NL_CALCLENGTH) return;
	}

	for (; sld->cmd != SL_END; sld++) {
		void *ptr = sld->global ? sld->address : GetVariableAddress(object, sld);
		SlObjectMember(ptr, sld);
	}
}

/**
 * Save or Load (a list of) global variables
 * @param sldg The global variable that is being loaded or saved
 */
void SlGlobList(const SaveLoadGlobVarList *sldg)
{
	SlObject(NULL, (const SaveLoad*)sldg);
}

/**
 * Do something of which I have no idea what it is :P
 * @param proc The callback procedure that is called
 * @param arg The variable that will be used for the callback procedure
 */
void SlAutolength(AutolengthProc *proc, void *arg)
{
	size_t offs;

	assert(_sl.action == SLA_SAVE);

	/* Tell it to calculate the length */
	_sl.need_length = NL_CALCLENGTH;
	_sl.obj_len = 0;
	proc(arg);

	/* Setup length */
	_sl.need_length = NL_WANTLENGTH;
	SlSetLength(_sl.obj_len);

	offs = SlGetOffs() + _sl.obj_len;

	/* And write the stuff */
	proc(arg);

	if (offs != SlGetOffs()) SlErrorCorrupt("Invalid chunk size");
}

/**
 * Load a chunk of data (eg vehicles, stations, etc.)
 * @param ch The chunkhandler that will be used for the operation
 */
static void SlLoadChunk(const ChunkHandler *ch)
{
	byte m = SlReadByte();
	size_t len;
	size_t endoffs;

	_sl.block_mode = m;
	_sl.obj_len = 0;

	switch (m) {
		case CH_ARRAY:
			_sl.array_index = 0;
			ch->load_proc();
			break;
		case CH_SPARSE_ARRAY:
			ch->load_proc();
			break;
		default:
			if ((m & 0xF) == CH_RIFF) {
				/* Read length */
				len = (SlReadByte() << 16) | ((m >> 4) << 24);
				len += SlReadUint16();
				_sl.obj_len = len;
				endoffs = SlGetOffs() + len;
				ch->load_proc();
				if (SlGetOffs() != endoffs) SlErrorCorrupt("Invalid chunk size");
			} else {
				SlErrorCorrupt("Invalid chunk type");
			}
			break;
	}
}

/**
 * Load a chunk of data for checking savegames.
 * If the chunkhandler is NULL, the chunk is skipped.
 * @param ch The chunkhandler that will be used for the operation
 */
static void SlLoadCheckChunk(const ChunkHandler *ch)
{
	byte m = SlReadByte();
	size_t len;
	size_t endoffs;

	_sl.block_mode = m;
	_sl.obj_len = 0;

	switch (m) {
		case CH_ARRAY:
			_sl.array_index = 0;
			if (ch->load_check_proc) {
				ch->load_check_proc();
			} else {
				SlSkipArray();
			}
			break;
		case CH_SPARSE_ARRAY:
			if (ch->load_check_proc) {
				ch->load_check_proc();
			} else {
				SlSkipArray();
			}
			break;
		default:
			if ((m & 0xF) == CH_RIFF) {
				/* Read length */
				len = (SlReadByte() << 16) | ((m >> 4) << 24);
				len += SlReadUint16();
				_sl.obj_len = len;
				endoffs = SlGetOffs() + len;
				if (ch->load_check_proc) {
					ch->load_check_proc();
				} else {
					SlSkipBytes(len);
				}
				if (SlGetOffs() != endoffs) SlErrorCorrupt("Invalid chunk size");
			} else {
				SlErrorCorrupt("Invalid chunk type");
			}
			break;
	}
}

/**
 * Stub Chunk handlers to only calculate length and do nothing else.
 * The intended chunk handler that should be called.
 */
static ChunkSaveLoadProc *_stub_save_proc;

/**
 * Stub Chunk handlers to only calculate length and do nothing else.
 * Actually call the intended chunk handler.
 * @param arg ignored parameter.
 */
static inline void SlStubSaveProc2(void *arg)
{
	_stub_save_proc();
}

/**
 * Stub Chunk handlers to only calculate length and do nothing else.
 * Call SlAutoLenth with our stub save proc that will eventually
 * call the intended chunk handler.
 */
static void SlStubSaveProc()
{
	SlAutolength(SlStubSaveProc2, NULL);
}

/**
 * Save a chunk of data (eg. vehicles, stations, etc.). Each chunk is
 * prefixed by an ID identifying it, followed by data, and terminator where appropiate
 * @param ch The chunkhandler that will be used for the operation
 */
static void SlSaveChunk(const ChunkHandler *ch)
{
	ChunkSaveLoadProc *proc = ch->save_proc;

	/* Don't save any chunk information if there is no save handler. */
	if (proc == NULL) return;

	SlWriteUint32(ch->id);
	DEBUG(sl, 2, "Saving chunk %c%c%c%c", ch->id >> 24, ch->id >> 16, ch->id >> 8, ch->id);

	if (ch->flags & CH_AUTO_LENGTH) {
		/* Need to calculate the length. Solve that by calling SlAutoLength in the save_proc. */
		_stub_save_proc = proc;
		proc = SlStubSaveProc;
	}

	_sl.block_mode = ch->flags & CH_TYPE_MASK;
	switch (ch->flags & CH_TYPE_MASK) {
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
		default: NOT_REACHED();
	}
}

/** Save all chunks */
static void SlSaveChunks()
{
	FOR_ALL_CHUNK_HANDLERS(ch) {
		SlSaveChunk(ch);
	}

	/* Terminator */
	SlWriteUint32(0);
}

/**
 * Find the ChunkHandler that will be used for processing the found
 * chunk in the savegame or in memory
 * @param id the chunk in question
 * @return returns the appropiate chunkhandler
 */
static const ChunkHandler *SlFindChunkHandler(uint32 id)
{
	FOR_ALL_CHUNK_HANDLERS(ch) if (ch->id == id) return ch;
	return NULL;
}

/** Load all chunks */
static void SlLoadChunks()
{
	uint32 id;
	const ChunkHandler *ch;

	for (id = SlReadUint32(); id != 0; id = SlReadUint32()) {
		DEBUG(sl, 2, "Loading chunk %c%c%c%c", id >> 24, id >> 16, id >> 8, id);

		ch = SlFindChunkHandler(id);
		if (ch == NULL) SlErrorCorrupt("Unknown chunk type");
		SlLoadChunk(ch);
	}
}

/** Load all chunks for savegame checking */
static void SlLoadCheckChunks()
{
	uint32 id;
	const ChunkHandler *ch;

	for (id = SlReadUint32(); id != 0; id = SlReadUint32()) {
		DEBUG(sl, 2, "Loading chunk %c%c%c%c", id >> 24, id >> 16, id >> 8, id);

		ch = SlFindChunkHandler(id);
		if (ch == NULL) SlErrorCorrupt("Unknown chunk type");
		SlLoadCheckChunk(ch);
	}
}

/** Fix all pointers (convert index -> pointer) */
static void SlFixPointers()
{
	_sl.action = SLA_PTRS;

	DEBUG(sl, 1, "Fixing pointers");

	FOR_ALL_CHUNK_HANDLERS(ch) {
		if (ch->ptrs_proc != NULL) {
			DEBUG(sl, 2, "Fixing pointers for %c%c%c%c", ch->id >> 24, ch->id >> 16, ch->id >> 8, ch->id);
			ch->ptrs_proc();
		}
	}

	DEBUG(sl, 1, "All pointers fixed");

	assert(_sl.action == SLA_PTRS);
}

/*******************************************
 ********** START OF LZO CODE **************
 *******************************************/

#ifdef WITH_LZO
#include <lzo/lzo1x.h>

/** Buffer size for the LZO compressor */
static const uint LZO_BUFFER_SIZE = 8192;

static size_t ReadLZO()
{
	/* Buffer size is from the LZO docs plus the chunk header size. */
	byte out[LZO_BUFFER_SIZE + LZO_BUFFER_SIZE / 16 + 64 + 3 + sizeof(uint32) * 2];
	uint32 tmp[2];
	uint32 size;
	lzo_uint len;

	/* Read header*/
	if (fread(tmp, sizeof(tmp), 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE, "File read failed");

	/* Check if size is bad */
	((uint32*)out)[0] = size = tmp[1];

	if (_sl_version != 0) {
		tmp[0] = TO_BE32(tmp[0]);
		size = TO_BE32(size);
	}

	if (size >= sizeof(out)) SlErrorCorrupt("Inconsistent size");

	/* Read block */
	if (fread(out + sizeof(uint32), size, 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE);

	/* Verify checksum */
	if (tmp[0] != lzo_adler32(0, out, size + sizeof(uint32))) SlErrorCorrupt("Bad checksum");

	/* Decompress */
	lzo1x_decompress(out + sizeof(uint32) * 1, size, _sl.buf, &len, NULL);
	return len;
}

static void WriteLZO(size_t size)
{
	const lzo_bytep in = _sl.buf;
	/* Buffer size is from the LZO docs plus the chunk header size. */
	byte out[LZO_BUFFER_SIZE + LZO_BUFFER_SIZE / 16 + 64 + 3 + sizeof(uint32) * 2];
	byte wrkmem[LZO1X_1_MEM_COMPRESS];
	lzo_uint outlen;

	do {
		/* Compress up to LZO_BUFFER_SIZE bytes at once. */
		lzo_uint len = size > LZO_BUFFER_SIZE ? LZO_BUFFER_SIZE : (lzo_uint)size;
		lzo1x_1_compress(in, len, out + sizeof(uint32) * 2, &outlen, wrkmem);
		((uint32*)out)[1] = TO_BE32((uint32)outlen);
		((uint32*)out)[0] = TO_BE32(lzo_adler32(0, out + sizeof(uint32), outlen + sizeof(uint32)));
		if (fwrite(out, outlen + sizeof(uint32) * 2, 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_WRITEABLE);

		/* Move to next data chunk. */
		size -= len;
		in += len;
	} while (size > 0);
}

static bool InitLZO(byte compression)
{
	if (lzo_init() != LZO_E_OK) return false;
	_sl.bufsize = LZO_BUFFER_SIZE;
	_sl.buf = _sl.buf_ori = MallocT<byte>(LZO_BUFFER_SIZE);
	return true;
}

static void UninitLZO()
{
	free(_sl.buf_ori);
}

#endif /* WITH_LZO */

/*********************************************
 ******** START OF NOCOMP CODE (uncompressed)*
 *********************************************/

/** Buffer size used for the uncompressing 'compressor' */
static const uint NOCOMP_BUFFER_SIZE = 8192;

static size_t ReadNoComp()
{
	return fread(_sl.buf, 1, NOCOMP_BUFFER_SIZE, _sl.fh);
}

static void WriteNoComp(size_t size)
{
	if (fwrite(_sl.buf, 1, size, _sl.fh) != size) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_WRITEABLE);
}

static bool InitNoComp(byte compression)
{
	_sl.bufsize = NOCOMP_BUFFER_SIZE;
	_sl.buf = _sl.buf_ori = MallocT<byte>(NOCOMP_BUFFER_SIZE);
	return true;
}

static void UninitNoComp()
{
	free(_sl.buf_ori);
}

/********************************************
 ********** START OF MEMORY CODE (in ram)****
 ********************************************/

#include "../gui.h"

struct ThreadedSave {
	uint count;
	byte ff_state;
	bool saveinprogress;
	CursorID cursor;
};

/** Save in chunks of 128 KiB. */
static const size_t MEMORY_CHUNK_SIZE = 128 * 1024;
/** Memory allocation for storing savegames in memory. */
static AutoFreeSmallVector<byte *, 16> _memory_savegame;

static ThreadedSave _ts;

static void WriteMem(size_t size)
{
	_ts.count += (uint)size;

	_sl.buf = CallocT<byte>(MEMORY_CHUNK_SIZE);
	*_memory_savegame.Append() = _sl.buf;
}

static void UnInitMem()
{
	_memory_savegame.Clear();
}

static bool InitMem()
{
	_ts.count = 0;
	_sl.bufsize = MEMORY_CHUNK_SIZE;

	UnInitMem();
	WriteMem(0);
	return true;
}

/********************************************
 ********** START OF ZLIB CODE **************
 ********************************************/

#if defined(WITH_ZLIB)
#include <zlib.h>

/** Buffer size for the LZO compressor */
static const uint ZLIB_BUFFER_SIZE = 8192;

static z_stream _z;

static bool InitReadZlib(byte compression)
{
	memset(&_z, 0, sizeof(_z));
	if (inflateInit(&_z) != Z_OK) return false;

	_sl.bufsize = ZLIB_BUFFER_SIZE;
	_sl.buf = _sl.buf_ori = MallocT<byte>(ZLIB_BUFFER_SIZE + ZLIB_BUFFER_SIZE); // also contains fread buffer
	return true;
}

static size_t ReadZlib()
{
	int r;

	_z.next_out = _sl.buf;
	_z.avail_out = ZLIB_BUFFER_SIZE;

	do {
		/* read more bytes from the file? */
		if (_z.avail_in == 0) {
			_z.avail_in = (uint)fread(_z.next_in = _sl.buf + ZLIB_BUFFER_SIZE, 1, ZLIB_BUFFER_SIZE, _sl.fh);
		}

		/* inflate the data */
		r = inflate(&_z, 0);
		if (r == Z_STREAM_END) break;

		if (r != Z_OK) SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "inflate() failed");
	} while (_z.avail_out);

	return ZLIB_BUFFER_SIZE - _z.avail_out;
}

static void UninitReadZlib()
{
	inflateEnd(&_z);
	free(_sl.buf_ori);
}

static bool InitWriteZlib(byte compression)
{
	memset(&_z, 0, sizeof(_z));
	if (deflateInit(&_z, compression) != Z_OK) return false;

	_sl.bufsize = ZLIB_BUFFER_SIZE;
	_sl.buf = _sl.buf_ori = MallocT<byte>(ZLIB_BUFFER_SIZE);
	return true;
}

static void WriteZlibLoop(z_streamp z, byte *p, size_t len, int mode)
{
	byte buf[ZLIB_BUFFER_SIZE]; // output buffer
	int r;
	uint n;
	z->next_in = p;
	z->avail_in = (uInt)len;
	do {
		z->next_out = buf;
		z->avail_out = sizeof(buf);

		/**
		 * For the poor next soul who sees many valgrind warnings of the
		 * "Conditional jump or move depends on uninitialised value(s)" kind:
		 * According to the author of zlib it is not a bug and it won't be fixed.
		 * http://groups.google.com/group/comp.compression/browse_thread/thread/b154b8def8c2a3ef/cdf9b8729ce17ee2
		 * [Mark Adler, Feb 24 2004, 'zlib-1.2.1 valgrind warnings' in the newgroup comp.compression]
		 */
		r = deflate(z, mode);

		/* bytes were emitted? */
		if ((n = sizeof(buf) - z->avail_out) != 0) {
			if (fwrite(buf, n, 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_WRITEABLE);
		}
		if (r == Z_STREAM_END) break;

		if (r != Z_OK) SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "zlib returned error code");
	} while (z->avail_in || !z->avail_out);
}

static void WriteZlib(size_t len)
{
	WriteZlibLoop(&_z, _sl.buf, len, 0);
}

static void UninitWriteZlib()
{
	/* flush any pending output. */
	if (_sl.fh) WriteZlibLoop(&_z, NULL, 0, Z_FINISH);
	deflateEnd(&_z);
	free(_sl.buf_ori);
}

#endif /* WITH_ZLIB */

/********************************************
 ********** START OF LZMA CODE **************
 ********************************************/

#if defined(WITH_LZMA)
#include <lzma.h>

/**
 * Have a copy of an initialised LZMA stream. We need this as it's
 * impossible to "re"-assign LZMA_STREAM_INIT to a variable, i.e.
 * LZMA_STREAM_INIT can't be used to reset something. This var can.
 */
static const lzma_stream _lzma_init = LZMA_STREAM_INIT;
/** The current LZMA stream we're processing. */
static lzma_stream _lzma;

static bool InitReadLZMA(byte compression)
{
	_lzma = _lzma_init;
	/* Allow saves up to 256 MB uncompressed */
	if (lzma_auto_decoder(&_lzma, 1 << 28, 0) != LZMA_OK) return false;

	_sl.bufsize = MEMORY_CHUNK_SIZE;
	_sl.buf = _sl.buf_ori = MallocT<byte>(MEMORY_CHUNK_SIZE + MEMORY_CHUNK_SIZE); // also contains fread buffer
	return true;
}

static size_t ReadLZMA()
{
	_lzma.next_out = _sl.buf;
	_lzma.avail_out = MEMORY_CHUNK_SIZE;

	do {
		/* read more bytes from the file? */
		if (_lzma.avail_in == 0) {
			_lzma.next_in = _sl.buf + MEMORY_CHUNK_SIZE;
			_lzma.avail_in = fread(_sl.buf + MEMORY_CHUNK_SIZE, 1, MEMORY_CHUNK_SIZE, _sl.fh);
		}

		/* inflate the data */
		lzma_ret r = lzma_code(&_lzma, LZMA_RUN);
		if (r == LZMA_STREAM_END) break;
		if (r != LZMA_OK) SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "liblzma returned error code");
	} while (_lzma.avail_out != 0);

	return MEMORY_CHUNK_SIZE - _lzma.avail_out;
}

static void UninitReadLZMA()
{
	lzma_end(&_lzma);
	free(_sl.buf_ori);
}

static bool InitWriteLZMA(byte compression)
{
	_lzma = _lzma_init;
	if (lzma_easy_encoder(&_lzma, compression, LZMA_CHECK_CRC32) != LZMA_OK) return false;

	_sl.bufsize = MEMORY_CHUNK_SIZE;
	_sl.buf = _sl.buf_ori = MallocT<byte>(MEMORY_CHUNK_SIZE);
	return true;
}

static void WriteLZMALoop(lzma_stream *lzma, byte *p, size_t len, lzma_action action)
{
	byte buf[MEMORY_CHUNK_SIZE]; // output buffer
	size_t n;
	lzma->next_in = p;
	lzma->avail_in = len;
	do {
		lzma->next_out = buf;
		lzma->avail_out = sizeof(buf);

		lzma_ret r = lzma_code(&_lzma, action);

		/* bytes were emitted? */
		if ((n = sizeof(buf) - lzma->avail_out) != 0) {
			if (fwrite(buf, n, 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_WRITEABLE);
		}
		if (r == LZMA_STREAM_END) break;
		if (r != LZMA_OK) SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "liblzma returned error code");
	} while (lzma->avail_in || !lzma->avail_out);
}

static void WriteLZMA(size_t len)
{
	WriteLZMALoop(&_lzma, _sl.buf, len, LZMA_RUN);
}

static void UninitWriteLZMA()
{
	/* flush any pending output. */
	if (_sl.fh) WriteLZMALoop(&_lzma, NULL, 0, LZMA_FINISH);
	lzma_end(&_lzma);
	free(_sl.buf_ori);
}

#endif /* WITH_LZMA */

/*******************************************
 ************* END OF CODE *****************
 *******************************************/

/** The format for a reader/writer type of a savegame */
struct SaveLoadFormat {
	const char *name;                     ///< name of the compressor/decompressor (debug-only)
	uint32 tag;                           ///< the 4-letter tag by which it is identified in the savegame

	bool (*init_read)(byte compression);  ///< function executed upon initalization of the loader
	ReaderProc *reader;                   ///< function that loads the data from the file
	void (*uninit_read)();                ///< function executed when reading is finished

	bool (*init_write)(byte compression); ///< function executed upon intialization of the saver
	WriterProc *writer;                   ///< function that saves the data to the file
	void (*uninit_write)();               ///< function executed when writing is done

	byte min_compression;                 ///< the minimum compression level of this format
	byte default_compression;             ///< the default compression level of this format
	byte max_compression;                 ///< the maximum compression level of this format
};

/** The different saveload formats known/understood by OpenTTD. */
static const SaveLoadFormat _saveload_formats[] = {
#if defined(WITH_LZO)
	/* Roughly 75% larger than zlib level 6 at only ~7% of the CPU usage. */
	{"lzo",    TO_BE32X('OTTD'), InitLZO,      ReadLZO,    UninitLZO,      InitLZO,       WriteLZO,    UninitLZO,       0, 0, 0},
#else
	{"lzo",    TO_BE32X('OTTD'), NULL,         NULL,       NULL,           NULL,          NULL,        NULL,            0, 0, 0},
#endif
	/* Roughly 5 times larger at only 1% of the CPU usage over zlib level 6. */
	{"none",   TO_BE32X('OTTN'), InitNoComp,   ReadNoComp, UninitNoComp,   InitNoComp,    WriteNoComp, UninitNoComp,    0, 0, 0},
#if defined(WITH_ZLIB)
	/* After level 6 the speed reduction is significant (1.5x to 2.5x slower per level), but the reduction in filesize is
	 * fairly insignificant (~1% for each step). Lower levels become ~5-10% bigger by each level than level 6 while level
	 * 1 is "only" 3 times as fast. Level 0 results in uncompressed savegames at about 8 times the cost of "none". */
	{"zlib",   TO_BE32X('OTTZ'), InitReadZlib, ReadZlib,   UninitReadZlib, InitWriteZlib, WriteZlib,   UninitWriteZlib, 0, 6, 9},
#else
	{"zlib",   TO_BE32X('OTTZ'), NULL,         NULL,       NULL,           NULL,          NULL,        NULL,            0, 0, 0},
#endif
#if defined(WITH_LZMA)
	/* Level 2 compression is speed wise as fast as zlib level 6 compression (old default), but results in ~10% smaller saves.
	 * Higher compression levels are possible, and might improve savegame size by up to 25%, but are also up to 10 times slower.
	 * The next significant reduction in file size is at level 4, but that is already 4 times slower. Level 3 is primarily 50%
	 * slower while not improving the filesize, while level 0 and 1 are faster, but don't reduce savegame size much.
	 * It's OTTX and not e.g. OTTL because liblzma is part of xz-utils and .tar.xz is prefered over .tar.lzma. */
	{"lzma",   TO_BE32X('OTTX'), InitReadLZMA, ReadLZMA,   UninitReadLZMA, InitWriteLZMA, WriteLZMA,   UninitWriteLZMA, 0, 2, 9},
#else
	{"lzma",   TO_BE32X('OTTX'), NULL,         NULL,       NULL,           NULL,          NULL,        NULL,            0, 0, 0},
#endif
};

/**
 * Return the savegameformat of the game. Whether it was created with ZLIB compression
 * uncompressed, or another type
 * @param s Name of the savegame format. If NULL it picks the first available one
 * @param compression_level Output for telling what compression level we want.
 * @return Pointer to SaveLoadFormat struct giving all characteristics of this type of savegame
 */
static const SaveLoadFormat *GetSavegameFormat(char *s, byte *compression_level)
{
	const SaveLoadFormat *def = lastof(_saveload_formats);

	/* find default savegame format, the highest one with which files can be written */
	while (!def->init_write) def--;

	if (!StrEmpty(s)) {
		/* Get the ":..." of the compression level out of the way */
		char *complevel = strrchr(s, ':');
		if (complevel != NULL) *complevel = '\0';

		for (const SaveLoadFormat *slf = &_saveload_formats[0]; slf != endof(_saveload_formats); slf++) {
			if (slf->init_write != NULL && strcmp(s, slf->name) == 0) {
				*compression_level = slf->default_compression;
				if (complevel != NULL) {
					/* There is a compression level in the string.
					 * First restore the : we removed to do proper name matching,
					 * then move the the begin of the actual version. */
					*complevel = ':';
					complevel++;

					/* Get the version and determine whether all went fine. */
					char *end;
					long level = strtol(complevel, &end, 10);
					if (end == complevel || level != Clamp(level, slf->min_compression, slf->max_compression)) {
						ShowInfoF("Compression level '%s' is not valid.", complevel);
					} else {
						*compression_level = level;
					}
				}
				return slf;
			}
		}

		ShowInfoF("Savegame format '%s' is not available. Reverting to '%s'.", s, def->name);

		/* Restore the string by adding the : back */
		if (complevel != NULL) *complevel = ':';
	}
	*compression_level = def->default_compression;
	return def;
}

/* actual loader/saver function */
void InitializeGame(uint size_x, uint size_y, bool reset_date, bool reset_settings);
extern bool AfterLoadGame();
extern bool LoadOldSaveGame(const char *file);

/** Small helper function to close the to be loaded savegame and signal error */
static inline SaveOrLoadResult AbortSaveLoad()
{
	if (_sl.fh != NULL) fclose(_sl.fh);

	_sl.fh = NULL;
	return SL_ERROR;
}

/**
 * Update the gui accordingly when starting saving
 * and set locks on saveload. Also turn off fast-forward cause with that
 * saving takes Aaaaages
 */
static void SaveFileStart()
{
	_ts.ff_state = _fast_forward;
	_fast_forward = 0;
	if (_cursor.sprite == SPR_CURSOR_MOUSE) SetMouseCursor(SPR_CURSOR_ZZZ, PAL_NONE);

	InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SAVELOAD_START);
	_ts.saveinprogress = true;
}

/** Update the gui accordingly when saving is done and release locks on saveload. */
static void SaveFileDone()
{
	if (_game_mode != GM_MENU) _fast_forward = _ts.ff_state;
	if (_cursor.sprite == SPR_CURSOR_ZZZ) SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SAVELOAD_FINISH);
	_ts.saveinprogress = false;
}

/** Set the error message from outside of the actual loading/saving of the game (AfterLoadGame and friends) */
void SetSaveLoadError(StringID str)
{
	_sl.error_str = str;
}

/** Get the string representation of the error message */
const char *GetSaveLoadErrorString()
{
	SetDParam(0, _sl.error_str);
	SetDParamStr(1, _sl.extra_msg);

	static char err_str[512];
	GetString(err_str, _sl.action == SLA_SAVE ? STR_ERROR_GAME_SAVE_FAILED : STR_ERROR_GAME_LOAD_FAILED, lastof(err_str));
	return err_str;
}

/** Show a gui message when saving has failed */
static void SaveFileError()
{
	SetDParamStr(0, GetSaveLoadErrorString());
	ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
	SaveFileDone();
}

/**
 * We have written the whole game into memory, _memory_savegame, now find
 * and appropiate compressor and start writing to file.
 */
static SaveOrLoadResult SaveFileToDisk(bool threaded)
{
	_sl.excpt_uninit = NULL;
	try {
		byte compression;
		const SaveLoadFormat *fmt = GetSavegameFormat(_savegame_format, &compression);

		/* We have written our stuff to memory, now write it to file! */
		uint32 hdr[2] = { fmt->tag, TO_BE32(SAVEGAME_VERSION << 16) };
		if (fwrite(hdr, sizeof(hdr), 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_WRITEABLE);

		if (!fmt->init_write(compression)) SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize compressor");

		uint i = 0;
		size_t t = _ts.count;

		if (_ts.count != _sl.offs_base) SlErrorCorrupt("Unexpected size of chunk");
		while (t >= MEMORY_CHUNK_SIZE) {
			_sl.buf = _memory_savegame[i++];
			fmt->writer(MEMORY_CHUNK_SIZE);
			t -= MEMORY_CHUNK_SIZE;
		}

		if (t != 0) {
			/* The last block is (almost) always not fully filled, so only write away
			 * as much data as it is in there */
			_sl.buf = _memory_savegame[i];

			assert(t == _ts.count % MEMORY_CHUNK_SIZE);
			fmt->writer(t);
		}

		fmt->uninit_write();
		if (_ts.count != _sl.offs_base) SlErrorCorrupt("Unexpected size of chunk");
		UnInitMem();
		fclose(_sl.fh);

		if (threaded) SetAsyncSaveFinish(SaveFileDone);

		return SL_OK;
	} catch (...) {
		AbortSaveLoad();
		if (_sl.excpt_uninit != NULL) _sl.excpt_uninit();

		/* Skip the "colour" character */
		DEBUG(sl, 0, "%s", GetSaveLoadErrorString() + 3);

		if (threaded) {
			SetAsyncSaveFinish(SaveFileError);
		} else {
			SaveFileError();
		}
		return SL_ERROR;
	}
}

static void SaveFileToDiskThread(void *arg)
{
	SaveFileToDisk(true);
}

void WaitTillSaved()
{
	if (_save_thread == NULL) return;

	_save_thread->Join();
	delete _save_thread;
	_save_thread = NULL;
}

/**
 * Main Save or Load function where the high-level saveload functions are
 * handled. It opens the savegame, selects format and checks versions
 * @param filename The name of the savegame being created/loaded
 * @param mode Save or load. Load can also be a TTD(Patch) game. Use SL_LOAD, SL_OLD_LOAD or SL_SAVE
 * @param sb The sub directory to save the savegame in
 * @param threaded True when threaded saving is allowed
 * @return Return the results of the action. SL_OK, SL_ERROR or SL_REINIT ("unload" the game)
 */
SaveOrLoadResult SaveOrLoad(const char *filename, int mode, Subdirectory sb, bool threaded)
{
	uint32 hdr[2];

	/* An instance of saving is already active, so don't go saving again */
	if (_ts.saveinprogress && mode == SL_SAVE) {
		/* if not an autosave, but a user action, show error message */
		if (!_do_autosave) ShowErrorMessage(STR_ERROR_SAVE_STILL_IN_PROGRESS, INVALID_STRING_ID, WL_ERROR);
		return SL_OK;
	}
	WaitTillSaved();

	/* Clear previous check data */
	if (mode == SL_LOAD_CHECK) _load_check_data.Clear();
	_next_offs = 0;

	/* Load a TTDLX or TTDPatch game */
	if (mode == SL_OLD_LOAD) {
		_engine_mngr.ResetToDefaultMapping();
		InitializeGame(256, 256, true, true); // set a mapsize of 256x256 for TTDPatch games or it might get confused

		/* TTD/TTO savegames have no NewGRFs, TTDP savegame have them
		 * and if so a new NewGRF list will be made in LoadOldSaveGame.
		 * Note: this is done here because AfterLoadGame is also called
		 * for OTTD savegames which have their own NewGRF logic. */
		ClearGRFConfigList(&_grfconfig);
		GamelogReset();
		if (!LoadOldSaveGame(filename)) return SL_REINIT;
		_sl_version = 0;
		_sl_minor_version = 0;
		GamelogStartAction(GLAT_LOAD);
		if (!AfterLoadGame()) {
			GamelogStopAction();
			return SL_REINIT;
		}
		GamelogStopAction();
		return SL_OK;
	}

	/* Mark SL_LOAD_CHECK as supported for this savegame. */
	if (mode == SL_LOAD_CHECK) _load_check_data.checkable = true;

	_sl.excpt_uninit = NULL;
	_sl.bufe = _sl.bufp = NULL;
	_sl.offs_base = 0;
	switch (mode) {
		case SL_LOAD_CHECK: _sl.action = SLA_LOAD_CHECK; break;
		case SL_LOAD: _sl.action = SLA_LOAD; break;
		case SL_SAVE: _sl.action = SLA_SAVE; break;
		default: NOT_REACHED();
	}

	try {
		_sl.fh = (mode == SL_SAVE) ? FioFOpenFile(filename, "wb", sb) : FioFOpenFile(filename, "rb", sb);

		/* Make it a little easier to load savegames from the console */
		if (_sl.fh == NULL && mode != SL_SAVE) _sl.fh = FioFOpenFile(filename, "rb", SAVE_DIR);
		if (_sl.fh == NULL && mode != SL_SAVE) _sl.fh = FioFOpenFile(filename, "rb", BASE_DIR);

		if (_sl.fh == NULL) {
			SlError(mode == SL_SAVE ? STR_GAME_SAVELOAD_ERROR_FILE_NOT_WRITEABLE : STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE);
		}

		/* General tactic is to first save the game to memory, then use an available writer
		 * to write it to file, either in threaded mode if possible, or single-threaded */
		if (mode == SL_SAVE) { // SAVE game
			DEBUG(desync, 1, "save: %08x; %02x; %s", _date, _date_fract, filename);

			_sl.write_bytes = WriteMem;
			_sl.excpt_uninit = UnInitMem;
			InitMem();

			_sl_version = SAVEGAME_VERSION;

			SaveViewportBeforeSaveGame();
			SlSaveChunks();
			SlWriteFill(); // flush the save buffer

			SaveFileStart();
			if (_network_server || !_settings_client.gui.threaded_saves) threaded = false;
			if (!threaded || !ThreadObject::New(&SaveFileToDiskThread, NULL, &_save_thread)) {
				if (threaded) DEBUG(sl, 1, "Cannot create savegame thread, reverting to single-threaded mode...");

				SaveOrLoadResult result = SaveFileToDisk(false);
				SaveFileDone();

				return result;
			}
		} else { // LOAD game
			assert(mode == SL_LOAD || mode == SL_LOAD_CHECK);
			DEBUG(desync, 1, "load: %s", filename);

			/* Can't fseek to 0 as in tar files that is not correct */
			long pos = ftell(_sl.fh);
			if (fread(hdr, sizeof(hdr), 1, _sl.fh) != 1) SlError(STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE);

			/* see if we have any loader for this type. */
			const SaveLoadFormat *fmt = _saveload_formats;
			for (;;) {
				/* No loader found, treat as version 0 and use LZO format */
				if (fmt == endof(_saveload_formats)) {
					DEBUG(sl, 0, "Unknown savegame type, trying to load it as the buggy format");
					clearerr(_sl.fh);
					fseek(_sl.fh, pos, SEEK_SET);
					_sl_version = 0;
					_sl_minor_version = 0;

					/* Try to find the LZO savegame format; it uses 'OTTD' as tag. */
					fmt = _saveload_formats;
					for (;;) {
						if (fmt == endof(_saveload_formats)) {
							/* Who removed LZO support? Bad bad boy! */
							NOT_REACHED();
						}
						if (fmt->tag == TO_BE32X('OTTD')) break;
						fmt++;
					}
					break;
				}

				if (fmt->tag == hdr[0]) {
					/* check version number */
					_sl_version = TO_BE32(hdr[1]) >> 16;
					/* Minor is not used anymore from version 18.0, but it is still needed
					 * in versions before that (4 cases) which can't be removed easy.
					 * Therefor it is loaded, but never saved (or, it saves a 0 in any scenario).
					 * So never EVER use this minor version again. -- TrueLight -- 22-11-2005 */
					_sl_minor_version = (TO_BE32(hdr[1]) >> 8) & 0xFF;

					DEBUG(sl, 1, "Loading savegame version %d", _sl_version);

					/* Is the version higher than the current? */
					if (_sl_version > SAVEGAME_VERSION) SlError(STR_GAME_SAVELOAD_ERROR_TOO_NEW_SAVEGAME);
					break;
				}

				fmt++;
			}

			_sl.read_bytes = fmt->reader;
			_sl.excpt_uninit = fmt->uninit_read;

			/* loader for this savegame type is not implemented? */
			if (fmt->init_read == NULL) {
				char err_str[64];
				snprintf(err_str, lengthof(err_str), "Loader for '%s' is not available.", fmt->name);
				SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, err_str);
			}

			if (!fmt->init_read(0)) {
				char err_str[64];
				snprintf(err_str, lengthof(err_str), "Initializing loader '%s' failed", fmt->name);
				SlError(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, err_str);
			}

			if (mode != SL_LOAD_CHECK) {
				_engine_mngr.ResetToDefaultMapping();

				/* Old maps were hardcoded to 256x256 and thus did not contain
				 * any mapsize information. Pre-initialize to 256x256 to not to
				 * confuse old games */
				InitializeGame(256, 256, true, true);

				GamelogReset();

				if (CheckSavegameVersion(4)) {
					/*
					 * NewGRFs were introduced between 0.3,4 and 0.3.5, which both
					 * shared savegame version 4. Anything before that 'obviously'
					 * does not have any NewGRFs. Between the introduction and
					 * savegame version 41 (just before 0.5) the NewGRF settings
					 * were not stored in the savegame and they were loaded by
					 * using the settings from the main menu.
					 * So, to recap:
					 * - savegame version  <  4:  do not load any NewGRFs.
					 * - savegame version >= 41:  load NewGRFs from savegame, which is
					 *                            already done at this stage by
					 *                            overwriting the main menu settings.
					 * - other savegame versions: use main menu settings.
					 *
					 * This means that users *can* crash savegame version 4..40
					 * savegames if they set incompatible NewGRFs in the main menu,
					 * but can't crash anymore for savegame version < 4 savegames.
					 *
					 * Note: this is done here because AfterLoadGame is also called
					 * for TTO/TTD/TTDP savegames which have their own NewGRF logic.
					 */
					ClearGRFConfigList(&_grfconfig);
				}
			}

			if (mode == SL_LOAD_CHECK) {
				/* Load chunks into _load_check_data.
				 * No pools are loaded. References are not possible, and thus do not need resolving. */
				SlLoadCheckChunks();
			} else {
				/* Load chunks and resolve references */
				SlLoadChunks();
				SlFixPointers();
			}
			fmt->uninit_read();
			fclose(_sl.fh);

			_savegame_type = SGT_OTTD;

			if (mode == SL_LOAD_CHECK) {
				/* The only part from AfterLoadGame() we need */
				_load_check_data.grf_compatibility = IsGoodGRFConfigList(_load_check_data.grfconfig);
			} else {
				GamelogStartAction(GLAT_LOAD);

				/* After loading fix up savegame for any internal changes that
				 * might have occurred since then. If it fails, load back the old game. */
				if (!AfterLoadGame()) {
					GamelogStopAction();
					return SL_REINIT;
				}

				GamelogStopAction();
			}
		}

		return SL_OK;
	} catch (...) {
		AbortSaveLoad();

		/* deinitialize compressor. */
		if (_sl.excpt_uninit != NULL) _sl.excpt_uninit();

		/* Skip the "colour" character */
		if (mode != SL_LOAD_CHECK) DEBUG(sl, 0, "%s", GetSaveLoadErrorString() + 3);

		/* A saver/loader exception!! reinitialize all variables to prevent crash! */
		return (mode == SL_LOAD) ? SL_REINIT : SL_ERROR;
	}
}

/** Do a save when exiting the game (_settings_client.gui.autosave_on_exit) */
void DoExitSave()
{
	SaveOrLoad("exit.sav", SL_SAVE, AUTOSAVE_DIR);
}

/**
 * Fill the buffer with the default name for a savegame *or* screenshot.
 * @param buf the buffer to write to.
 * @param last the last element in the buffer.
 */
void GenerateDefaultSaveName(char *buf, const char *last)
{
	/* Check if we have a name for this map, which is the name of the first
	 * available company. When there's no company available we'll use
	 * 'Spectator' as "company" name. */
	CompanyID cid = _local_company;
	if (!Company::IsValidID(cid)) {
		const Company *c;
		FOR_ALL_COMPANIES(c) {
			cid = c->index;
			break;
		}
	}

	SetDParam(0, cid);

	/* Insert current date */
	switch (_settings_client.gui.date_format_in_default_names) {
		case 0: SetDParam(1, STR_JUST_DATE_LONG); break;
		case 1: SetDParam(1, STR_JUST_DATE_TINY); break;
		case 2: SetDParam(1, STR_JUST_DATE_ISO); break;
		default: NOT_REACHED();
	}
	SetDParam(2, _date);

	/* Get the correct string (special string for when there's not company) */
	GetString(buf, !Company::IsValidID(cid) ? STR_SAVEGAME_NAME_SPECTATOR : STR_SAVEGAME_NAME_DEFAULT, last);
	SanitizeFilename(buf);
}

#if 0
/**
 * Function to get the type of the savegame by looking at the file header.
 * NOTICE: Not used right now, but could be used if extensions of savegames are garbled
 * @param file Savegame to be checked
 * @return SL_OLD_LOAD or SL_LOAD of the file
 */
int GetSavegameType(char *file)
{
	const SaveLoadFormat *fmt;
	uint32 hdr;
	FILE *f;
	int mode = SL_OLD_LOAD;

	f = fopen(file, "rb");
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		DEBUG(sl, 0, "Savegame is obsolete or invalid format");
		mode = SL_LOAD; // don't try to get filename, just show name as it is written
	} else {
		/* see if we have any loader for this type. */
		for (fmt = _saveload_formats; fmt != endof(_saveload_formats); fmt++) {
			if (fmt->tag == hdr) {
				mode = SL_LOAD; // new type of savegame
				break;
			}
		}
	}

	fclose(f);
	return mode;
}
#endif
