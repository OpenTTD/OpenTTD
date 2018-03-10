/* $Id$ */

/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

/* @file midifile.cpp Parser for standard MIDI files */

#include "midifile.hpp"
#include "../fileio_func.h"
#include "../fileio_type.h"
#include "../core/endian_func.hpp"
#include "midi.h"
#include <algorithm>


/* implementation based on description at: http://www.somascape.org/midi/tech/mfile.html */


/**
 * Owning byte buffer readable as a stream.
 * RAII-compliant to make teardown in error situations easier.
 */
class ByteBuffer {
	byte *buf;
	size_t buflen;
	size_t pos;
public:
	/**
	 * Construct buffer from data in a file.
	 * If file does not have sufficient bytes available, the object is constructed
	 * in an error state, that causes all further function calls to fail.
	 * @param file file to read from at current position
	 * @param len number of bytes to read
	 */
	ByteBuffer(FILE *file, size_t len)
	{
		this->buf = MallocT<byte>(len);
		if (fread(this->buf, 1, len, file) == len) {
			this->buflen = len;
			this->pos = 0;
		} else {
			/* invalid state */
			this->buflen = 0;
		}
	}

	/**
	 * Destructor, frees the buffer.
	 */
	~ByteBuffer()
	{
		free(this->buf);
	}

	/**
	 * Return whether the buffer was constructed successfully.
	 * @return true is the buffer contains data
	 */
	bool IsValid() const
	{
		return this->buflen > 0;
	}

	/**
	 * Return whether reading has reached the end of the buffer.
	 * @return true if there are no more bytes available to read
	 */
	bool IsEnd() const
	{
		return this->pos >= this->buflen;
	}

	/**
	 * Read a single byte from the buffer.
	 * @param[out] b returns the read value
	 * @return true if a byte was available for reading
	 */
	bool ReadByte(byte &b)
	{
		if (this->IsEnd()) return false;
		b = this->buf[this->pos++];
		return true;
	}

	/**
	 * Read a MIDI file variable length value.
	 * Each byte encodes 7 bits of the value, most-significant bits are encoded first.
	 * If the most significant bit in a byte is set, there are further bytes encoding the value.
	 * @param[out] res returns the read value
	 * @return true if there was data available
	 */
	bool ReadVariableLength(uint32 &res)
	{
		res = 0;
		byte b = 0;
		do {
			if (this->IsEnd()) return false;
			b = this->buf[this->pos++];
			res = (res << 7) | (b & 0x7F);
		} while (b & 0x80);
		return true;
	}

	/**
	 * Read bytes into a buffer.
	 * @param[out] dest buffer to copy info
	 * @param length number of bytes to read
	 * @return true if the requested number of bytes were available
	 */
	bool ReadBuffer(byte *dest, size_t length)
	{
		if (this->IsEnd()) return false;
		if (this->buflen - this->pos < length) return false;
		memcpy(dest, this->buf + this->pos, length);
		this->pos += length;
		return true;
	}

	/**
	 * Skip over a number of bytes in the buffer.
	 * @param count number of bytes to skip over
	 * @return true if there were enough bytes available
	 */
	bool Skip(size_t count)
	{
		if (this->IsEnd()) return false;
		if (this->buflen - this->pos < count) return false;
		this->pos += count;
		return true;
	}

	/**
	 * Go a number of bytes back to re-read.
	 * @param count number of bytes to go back
	 * @return true if at least count bytes had been read previously
	 */
	bool Rewind(size_t count)
	{
		if (count > this->pos) return false;
		this->pos -= count;
		return true;
	}
};

static bool ReadTrackChunk(FILE *file, MidiFile &target)
{
	byte buf[4];

	const byte magic[] = { 'M', 'T', 'r', 'k' };
	if (fread(buf, sizeof(magic), 1, file) != 1) {
		return false;
	}
	if (memcmp(magic, buf, sizeof(magic)) != 0) {
		return false;
	}

	/* read chunk length and then the whole chunk */
	uint32 chunk_length;
	if (fread(&chunk_length, 1, 4, file) != 4) {
		return false;
	}
	chunk_length = FROM_BE32(chunk_length);

	ByteBuffer chunk(file, chunk_length);
	if (!chunk.IsValid()) {
		return false;
	}

	target.blocks.push_back(MidiFile::DataBlock());
	MidiFile::DataBlock *block = &target.blocks.back();

	byte last_status = 0;
	bool running_sysex = false;
	while (!chunk.IsEnd()) {
		/* read deltatime for event, start new block */
		uint32 deltatime = 0;
		if (!chunk.ReadVariableLength(deltatime)) {
			return false;
		}
		if (deltatime > 0) {
			target.blocks.push_back(MidiFile::DataBlock(block->ticktime + deltatime));
			block = &target.blocks.back();
		}

		/* read status byte */
		byte status;
		if (!chunk.ReadByte(status)) {
			return false;
		}

		if ((status & 0x80) == 0) {
			/* high bit not set means running status message, status is same as last
			 * convert to explicit status */
			chunk.Rewind(1);
			status = last_status;
			goto running_status;
		} else if ((status & 0xF0) != 0xF0) {
			/* Regular channel message */
			last_status = status;
		running_status:
			byte *data;
			switch (status & 0xF0) {
				case MIDIST_NOTEOFF:
				case MIDIST_NOTEON:
				case MIDIST_POLYPRESS:
				case MIDIST_CONTROLLER:
				case MIDIST_PITCHBEND:
					/* 3 byte messages */
					data = block->data.Append(3);
					data[0] = status;
					if (!chunk.ReadBuffer(&data[1], 2)) {
						return false;
					}
					break;
				case MIDIST_PROGCHG:
				case MIDIST_CHANPRESS:
					/* 2 byte messages */
					data = block->data.Append(2);
					data[0] = status;
					if (!chunk.ReadByte(data[1])) {
						return false;
					}
					break;
				default:
					NOT_REACHED();
			}
		} else if (status == MIDIST_SMF_META) {
			/* Meta event, read event type byte and data length */
			if (!chunk.ReadByte(buf[0])) {
				return false;
			}
			uint32 length = 0;
			if (!chunk.ReadVariableLength(length)) {
				return false;
			}
			switch (buf[0]) {
				case 0x2F:
					/* end of track, no more data (length != 0 is illegal) */
					return (length == 0);
				case 0x51:
					/* tempo change */
					if (length != 3) return false;
					if (!chunk.ReadBuffer(buf, 3)) return false;
					target.tempos.push_back(MidiFile::TempoChange(block->ticktime, buf[0] << 16 | buf[1] << 8 | buf[2]));
					break;
				default:
					/* unimportant meta event, skip over it */
					if (!chunk.Skip(length)) {
						return false;
					}
					break;
			}
		} else if (status == MIDIST_SYSEX || (status == MIDIST_SMF_ESCAPE  && running_sysex)) {
			/* System exclusive message */
			uint32 length = 0;
			if (!chunk.ReadVariableLength(length)) {
				return false;
			}
			byte *data = block->data.Append(length + 1);
			data[0] = 0xF0;
			if (!chunk.ReadBuffer(data + 1, length)) {
				return false;
			}
			if (data[length] != 0xF7) {
				/* engage Casio weirdo mode - convert to normal sysex */
				running_sysex = true;
				*block->data.Append() = 0xF7;
			} else {
				running_sysex = false;
			}
		} else if (status == MIDIST_SMF_ESCAPE) {
			/* Escape sequence */
			uint32 length = 0;
			if (!chunk.ReadVariableLength(length)) {
				return false;
			}
			byte *data = block->data.Append(length);
			if (!chunk.ReadBuffer(data, length)) {
				return false;
			}
		} else {
			/* Messages undefined in standard midi files:
			 * 0xF1 - MIDI time code quarter frame
			 * 0xF2 - Song position pointer
			 * 0xF3 - Song select
			 * 0xF4 - undefined/reserved
			 * 0xF5 - undefined/reserved
			 * 0xF6 - Tune request for analog synths
			 * 0xF8..0xFE - System real-time messages
			 */
			return false;
		}
	}

	NOT_REACHED();
}

template<typename T>
bool TicktimeAscending(const T &a, const T &b)
{
	return a.ticktime < b.ticktime;
}

static bool FixupMidiData(MidiFile &target)
{
	/* Sort all tempo changes and events */
	std::sort(target.tempos.begin(), target.tempos.end(), TicktimeAscending<MidiFile::TempoChange>);
	std::sort(target.blocks.begin(), target.blocks.end(), TicktimeAscending<MidiFile::DataBlock>);

	if (target.tempos.size() == 0) {
		/* no tempo information, assume 120 bpm (500,000 microseconds per beat */
		target.tempos.push_back(MidiFile::TempoChange(0, 500000));
	}
	/* add sentinel tempo at end */
	target.tempos.push_back(MidiFile::TempoChange(UINT32_MAX, 0));

	/* merge blocks with identical tick times */
	std::vector<MidiFile::DataBlock> merged_blocks;
	uint32 last_ticktime = 0;
	for (size_t i = 0; i < target.blocks.size(); i++) {
		MidiFile::DataBlock &block = target.blocks[i];
		if (block.ticktime > last_ticktime || merged_blocks.size() == 0) {
			merged_blocks.push_back(block);
			last_ticktime = block.ticktime;
		} else {
			byte *datadest = merged_blocks.back().data.Append(block.data.Length());
			memcpy(datadest, block.data.Begin(), block.data.Length());
		}
	}
	std::swap(merged_blocks, target.blocks);

	/* annotate blocks with real time */
	last_ticktime = 0;
	uint32 last_realtime = 0;
	size_t cur_tempo = 0, cur_block = 0;
	while (cur_block < target.blocks.size()) {
		MidiFile::DataBlock &block = target.blocks[cur_block];
		MidiFile::TempoChange &tempo = target.tempos[cur_tempo];
		MidiFile::TempoChange &next_tempo = target.tempos[cur_tempo+1];
		if (block.ticktime <= next_tempo.ticktime) {
			/* block is within the current tempo */
			int64 tickdiff = block.ticktime - last_ticktime;
			last_ticktime = block.ticktime;
			last_realtime += uint32(tickdiff * tempo.tempo / target.tickdiv);
			block.realtime = last_realtime;
			cur_block++;
		} else {
			/* tempo change occurs before this block */
			int64 tickdiff = next_tempo.ticktime - last_ticktime;
			last_ticktime = next_tempo.ticktime;
			last_realtime += uint32(tickdiff * tempo.tempo / target.tickdiv); // current tempo until the tempo change
			cur_tempo++;
		}
	}

	return true;
}

/**
 * Read the header of a standard MIDI file.
 * @param[in] filename name of file to read from
 * @param[out] header filled with data read
 * @return true if the file could be opened and contained a header with correct format
 */
bool MidiFile::ReadSMFHeader(const char *filename, SMFHeader &header)
{
	FILE *file = FioFOpenFile(filename, "rb", Subdirectory::BASESET_DIR);
	if (!file) return false;
	bool result = ReadSMFHeader(file, header);
	FioFCloseFile(file);
	return result;
}

/**
 * Read the header of a standard MIDI file.
 * The function will consume 14 bytes from the current file pointer position.
 * @param[in] file open file to read from (should be in binary mode)
 * @param[out] header filled with data read
 * @return true if a header in correct format could be read from the file
 */
bool MidiFile::ReadSMFHeader(FILE *file, SMFHeader &header)
{
	/* Try to read header, fixed size */
	byte buffer[14];
	if (fread(buffer, sizeof(buffer), 1, file) != 1) {
		return false;
	}

	/* check magic, 'MThd' followed by 4 byte length indicator (always = 6 in SMF) */
	const byte magic[] = { 'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06 };
	if (MemCmpT(buffer, magic, sizeof(magic)) != 0) {
		return false;
	}

	/* read the parameters of the file */
	header.format = (buffer[8] << 8) | buffer[9];
	header.tracks = (buffer[10] << 8) | buffer[11];
	header.tickdiv = (buffer[12] << 8) | buffer[13];
	return true;
}

/**
 * Load a standard MIDI file.
 * @param filename name of the file to load
 * @returns true if loaded was successful
 */
bool MidiFile::LoadFile(const char *filename)
{
	this->blocks.clear();
	this->tempos.clear();
	this->tickdiv = 0;

	bool success = false;
	FILE *file = FioFOpenFile(filename, "rb", Subdirectory::BASESET_DIR);

	SMFHeader header;
	if (!ReadSMFHeader(file, header)) goto cleanup;

	/* Only format 0 (single-track) and format 1 (multi-track single-song) are accepted for now */
	if (header.format != 0 && header.format != 1) goto cleanup;
	/* Doesn't support SMPTE timecode files */
	if ((header.tickdiv & 0x8000) != 0) goto cleanup;

	this->tickdiv = header.tickdiv;

	for (; header.tracks > 0; header.tracks--) {
		if (!ReadTrackChunk(file, *this)) {
			goto cleanup;
		}
	}

	success = FixupMidiData(*this);

cleanup:
	FioFCloseFile(file);
	return success;
}

/**
 * Move data from other to this, and clears other.
 * @param other object containing loaded data to take over
 */
void MidiFile::MoveFrom(MidiFile &other)
{
	std::swap(this->blocks, other.blocks);
	std::swap(this->tempos, other.tempos);
	this->tickdiv = other.tickdiv;

	other.blocks.clear();
	other.tempos.clear();
	other.tickdiv = 0;
}

