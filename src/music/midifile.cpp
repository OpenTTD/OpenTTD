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
#include "../string_func.h"
#include "../core/endian_func.hpp"
#include "../core/mem_func.hpp"
#include "../base_media_base.h"
#include "midi.h"

#include "../console_func.h"
#include "../console_internal.h"

/* SMF reader based on description at: http://www.somascape.org/midi/tech/mfile.html */


static MidiFile *_midifile_instance = nullptr;

/**
 * Retrieve a well-known MIDI system exclusive message.
 * @param msg Which sysex message to retrieve
 * @param[out] length Receives the length of the returned buffer
 * @return Pointer to byte buffer with sysex message
 */
const byte *MidiGetStandardSysexMessage(MidiSysexMessage msg, size_t &length)
{
	static byte reset_gm_sysex[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
	static byte reset_gs_sysex[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };
	static byte reset_xg_sysex[] = { 0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7 };
	static byte roland_reverb_sysex[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x01, 0x30, 0x02, 0x04, 0x00, 0x40, 0x40, 0x00, 0x00, 0x09, 0xF7 };

	switch (msg) {
		case MidiSysexMessage::ResetGM:
			length = lengthof(reset_gm_sysex);
			return reset_gm_sysex;
		case MidiSysexMessage::ResetGS:
			length = lengthof(reset_gs_sysex);
			return reset_gs_sysex;
		case MidiSysexMessage::ResetXG:
			length = lengthof(reset_xg_sysex);
			return reset_xg_sysex;
		case MidiSysexMessage::RolandSetReverb:
			length = lengthof(roland_reverb_sysex);
			return roland_reverb_sysex;
		default:
			NOT_REACHED();
	}
}

/**
 * Owning byte buffer readable as a stream.
 * RAII-compliant to make teardown in error situations easier.
 */
class ByteBuffer {
	std::vector<byte> buf;
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
		this->buf.resize(len);
		if (fread(this->buf.data(), 1, len, file) == len) {
			this->pos = 0;
		} else {
			/* invalid state */
			this->buf.clear();
		}
	}

	/**
	 * Return whether the buffer was constructed successfully.
	 * @return true is the buffer contains data
	 */
	bool IsValid() const
	{
		return !this->buf.empty();
	}

	/**
	 * Return whether reading has reached the end of the buffer.
	 * @return true if there are no more bytes available to read
	 */
	bool IsEnd() const
	{
		return this->pos >= this->buf.size();
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
	bool ReadVariableLength(uint32_t &res)
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
	 * @param[out] dest buffer to copy into
	 * @param length number of bytes to read
	 * @return true if the requested number of bytes were available
	 */
	bool ReadBuffer(byte *dest, size_t length)
	{
		if (this->IsEnd()) return false;
		if (this->buf.size() - this->pos < length) return false;
		std::copy(std::begin(this->buf) + this->pos, std::begin(this->buf) + this->pos + length, dest);
		this->pos += length;
		return true;
	}

	/**
	 * Read bytes into a MidiFile::DataBlock.
	 * @param[out] dest DataBlock to copy into
	 * @param length number of bytes to read
	 * @return true if the requested number of bytes were available
	 */
	bool ReadDataBlock(MidiFile::DataBlock *dest, size_t length)
	{
		if (this->IsEnd()) return false;
		if (this->buf.size() - this->pos < length) return false;
		dest->data.insert(dest->data.end(), std::begin(this->buf) + this->pos, std::begin(this->buf) + this->pos + length);
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
		if (this->buf.size() - this->pos < count) return false;
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

	/* Read chunk length and then the whole chunk */
	uint32_t chunk_length;
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
		/* Read deltatime for event, start new block */
		uint32_t deltatime = 0;
		if (!chunk.ReadVariableLength(deltatime)) {
			return false;
		}
		if (deltatime > 0) {
			target.blocks.push_back(MidiFile::DataBlock(block->ticktime + deltatime));
			block = &target.blocks.back();
		}

		/* Read status byte */
		byte status;
		if (!chunk.ReadByte(status)) {
			return false;
		}

		if ((status & 0x80) == 0) {
			/* High bit not set means running status message, status is same as last
			 * convert to explicit status */
			chunk.Rewind(1);
			status = last_status;
			goto running_status;
		} else if ((status & 0xF0) != 0xF0) {
			/* Regular channel message */
			last_status = status;
		running_status:
			switch (status & 0xF0) {
				case MIDIST_NOTEOFF:
				case MIDIST_NOTEON:
				case MIDIST_POLYPRESS:
				case MIDIST_CONTROLLER:
				case MIDIST_PITCHBEND:
					/* 3 byte messages */
					block->data.push_back(status);
					if (!chunk.ReadDataBlock(block, 2)) {
						return false;
					}
					break;
				case MIDIST_PROGCHG:
				case MIDIST_CHANPRESS:
					/* 2 byte messages */
					block->data.push_back(status);
					if (!chunk.ReadByte(buf[0])) {
						return false;
					}
					block->data.push_back(buf[0]);
					break;
				default:
					NOT_REACHED();
			}
		} else if (status == MIDIST_SMF_META) {
			/* Meta event, read event type byte and data length */
			if (!chunk.ReadByte(buf[0])) {
				return false;
			}
			uint32_t length = 0;
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
			uint32_t length = 0;
			if (!chunk.ReadVariableLength(length)) {
				return false;
			}
			block->data.push_back(0xF0);
			if (!chunk.ReadDataBlock(block, length)) {
				return false;
			}
			if (block->data.back() != 0xF7) {
				/* Engage Casio weirdo mode - convert to normal sysex */
				running_sysex = true;
				block->data.push_back(0xF7);
			} else {
				running_sysex = false;
			}
		} else if (status == MIDIST_SMF_ESCAPE) {
			/* Escape sequence */
			uint32_t length = 0;
			if (!chunk.ReadVariableLength(length)) {
				return false;
			}
			if (!chunk.ReadDataBlock(block, length)) {
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

	if (target.tempos.empty()) {
		/* No tempo information, assume 120 bpm (500,000 microseconds per beat */
		target.tempos.push_back(MidiFile::TempoChange(0, 500000));
	}
	/* Add sentinel tempo at end */
	target.tempos.push_back(MidiFile::TempoChange(UINT32_MAX, 0));

	/* Merge blocks with identical tick times */
	std::vector<MidiFile::DataBlock> merged_blocks;
	uint32_t last_ticktime = 0;
	for (size_t i = 0; i < target.blocks.size(); i++) {
		MidiFile::DataBlock &block = target.blocks[i];
		if (block.data.empty()) {
			continue;
		} else if (block.ticktime > last_ticktime || merged_blocks.empty()) {
			merged_blocks.push_back(block);
			last_ticktime = block.ticktime;
		} else {
			merged_blocks.back().data.insert(merged_blocks.back().data.end(), block.data.begin(), block.data.end());
		}
	}
	std::swap(merged_blocks, target.blocks);

	/* Annotate blocks with real time */
	last_ticktime = 0;
	uint32_t last_realtime = 0;
	size_t cur_tempo = 0, cur_block = 0;
	while (cur_block < target.blocks.size()) {
		MidiFile::DataBlock &block = target.blocks[cur_block];
		MidiFile::TempoChange &tempo = target.tempos[cur_tempo];
		MidiFile::TempoChange &next_tempo = target.tempos[cur_tempo + 1];
		if (block.ticktime <= next_tempo.ticktime) {
			/* block is within the current tempo */
			int64_t tickdiff = block.ticktime - last_ticktime;
			last_ticktime = block.ticktime;
			last_realtime += uint32_t(tickdiff * tempo.tempo / target.tickdiv);
			block.realtime = last_realtime;
			cur_block++;
		} else {
			/* tempo change occurs before this block */
			int64_t tickdiff = next_tempo.ticktime - last_ticktime;
			last_ticktime = next_tempo.ticktime;
			last_realtime += uint32_t(tickdiff * tempo.tempo / target.tickdiv); // current tempo until the tempo change
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
bool MidiFile::ReadSMFHeader(const std::string &filename, SMFHeader &header)
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

	/* Check magic, 'MThd' followed by 4 byte length indicator (always = 6 in SMF) */
	const byte magic[] = { 'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06 };
	if (MemCmpT(buffer, magic, sizeof(magic)) != 0) {
		return false;
	}

	/* Read the parameters of the file */
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
bool MidiFile::LoadFile(const std::string &filename)
{
	_midifile_instance = this;

	this->blocks.clear();
	this->tempos.clear();
	this->tickdiv = 0;

	bool success = false;
	FILE *file = FioFOpenFile(filename, "rb", Subdirectory::BASESET_DIR);
	if (file == nullptr) return false;

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
 * Decoder for "MPS MIDI" format data.
 * This format for MIDI music is also used in a few other Microprose games contemporary with Transport Tycoon.
 *
 * The song data are usually packed inside a CAT file, with one CAT chunk per song. The song titles are used as names for the CAT chunks.
 *
 * Unlike the Standard MIDI File format, which is based on the IFF structure, the MPS MIDI format is best described as two linked lists of sub-tracks,
 * the first list contains a number of reusable "segments", and the second list contains the "master tracks". Each list is prefixed with a byte
 * giving the number of elements in the list, and the actual list is just a byte count (BE16 format) for the segment/track followed by the actual data,
 * there is no index as such, so the entire data must be seeked through to build an index.
 *
 * The actual MIDI data inside each track is almost standard MIDI, prefixing every event with a delay, encoded using the same variable-length format
 * used in SMF. A few status codes have changed meaning in MPS MIDI: 0xFE changes control from master track to a segment, 0xFD returns from a segment
 * to the master track, and 0xFF is used to end the song. (In Standard MIDI all those values must only occur in real-time data.)
 *
 * As implemented in the original decoder, there is no support for recursively calling segments from segments, i.e. code 0xFE must only occur in
 * a master track, and code 0xFD must only occur in a segment. There are no checks made for this, it's assumed that the only input data will ever
 * be the original game music, not music from other games, or new productions.
 *
 * Additionally, some program change and controller events are given special meaning, see comments in the code.
 */
struct MpsMachine {
	/** Starting parameter and playback status for one channel/track */
	struct Channel {
		byte cur_program;    ///< program selected, used for velocity scaling (lookup into programvelocities array)
		byte running_status; ///< last midi status code seen
		uint16_t delay;        ///< frames until next command
		uint32_t playpos;      ///< next byte to play this channel from
		uint32_t startpos;     ///< start position of master track
		uint32_t returnpos;    ///< next return position after playing a segment
		Channel() : cur_program(0xFF), running_status(0), delay(0), playpos(0), startpos(0), returnpos(0) { }
	};
	Channel channels[16];         ///< playback status for each MIDI channel
	std::vector<uint32_t> segments; ///< pointers into songdata to repeatable data segments
	int16_t tempo_ticks;            ///< ticker that increments when playing a frame, decrements before playing a frame
	int16_t current_tempo;          ///< threshold for actually playing a frame
	int16_t initial_tempo;          ///< starting tempo of song
	bool shouldplayflag;          ///< not-end-of-song flag

	static const int TEMPO_RATE;
	static const byte programvelocities[128];

	const byte *songdata; ///< raw data array
	size_t songdatalen;   ///< length of song data
	MidiFile &target;     ///< recipient of data

	/** Overridden MIDI status codes used in the data format */
	enum MpsMidiStatus {
		MPSMIDIST_SEGMENT_RETURN = 0xFD, ///< resume playing master track from stored position
		MPSMIDIST_SEGMENT_CALL   = 0xFE, ///< store current position of master track playback, and begin playback of a segment
		MPSMIDIST_ENDSONG        = 0xFF, ///< immediately end the song
	};

	static void AddMidiData(MidiFile::DataBlock &block, byte b1, byte b2)
	{
		block.data.push_back(b1);
		block.data.push_back(b2);
	}
	static void AddMidiData(MidiFile::DataBlock &block, byte b1, byte b2, byte b3)
	{
		block.data.push_back(b1);
		block.data.push_back(b2);
		block.data.push_back(b3);
	}

	/**
	 * Construct a TTD DOS music format decoder.
	 * @param data Buffer of song data from CAT file, ownership remains with caller
	 * @param length Length of the data buffer in bytes
	 * @param target MidiFile object to add decoded data to
	 */
	MpsMachine(const byte *data, size_t length, MidiFile &target)
		: songdata(data), songdatalen(length), target(target)
	{
		uint32_t pos = 0;
		int loopmax;
		int loopidx;

		/* First byte is the initial "tempo" */
		this->initial_tempo = this->songdata[pos++];

		/* Next byte is a count of callable segments */
		loopmax = this->songdata[pos++];
		for (loopidx = 0; loopidx < loopmax; loopidx++) {
			/* Segments form a linked list in the stream,
			 * first two bytes in each is an offset to the next.
			 * Two bytes between offset to next and start of data
			 * are unaccounted for. */
			this->segments.push_back(pos + 4);
			pos += FROM_LE16(*(const int16_t *)(this->songdata + pos));
		}

		/* After segments follows list of master tracks for each channel,
		 * also prefixed with a byte counting actual tracks. */
		loopmax = this->songdata[pos++];
		for (loopidx = 0; loopidx < loopmax; loopidx++) {
			/* Similar structure to segments list, but also has
			 * the MIDI channel number as a byte before the offset
			 * to next track. */
			byte ch = this->songdata[pos++];
			this->channels[ch].startpos = pos + 4;
			pos += FROM_LE16(*(const int16_t *)(this->songdata + pos));
		}
	}

	/**
	 * Read an SMF-style variable length value (note duration) from songdata.
	 * @param pos Position to read from, updated to point to next byte after the value read
	 * @return Value read from data stream
	 */
	uint16_t ReadVariableLength(uint32_t &pos)
	{
		byte b = 0;
		uint16_t res = 0;
		do {
			b = this->songdata[pos++];
			res = (res << 7) + (b & 0x7F);
		} while (b & 0x80);
		return res;
	}

	/**
	 * Prepare for playback from the beginning. Resets the song pointer for every track to the beginning.
	 */
	void RestartSong()
	{
		for (int ch = 0; ch < 16; ch++) {
			Channel &chandata = this->channels[ch];
			if (chandata.startpos != 0) {
				/* Active track, set position to beginning */
				chandata.playpos = chandata.startpos;
				chandata.delay = this->ReadVariableLength(chandata.playpos);
			} else {
				/* Inactive track, mark as such */
				chandata.playpos = 0;
				chandata.delay = 0;
			}
		}
	}

	/**
	 * Play one frame of data from one channel
	 */
	uint16_t PlayChannelFrame(MidiFile::DataBlock &outblock, int channel)
	{
		uint16_t newdelay = 0;
		byte b1, b2;
		Channel &chandata = this->channels[channel];

		do {
			/* Read command/status byte */
			b1 = this->songdata[chandata.playpos++];

			/* Command 0xFE, call segment from master track */
			if (b1 == MPSMIDIST_SEGMENT_CALL) {
				b1 = this->songdata[chandata.playpos++];
				chandata.returnpos = chandata.playpos;
				chandata.playpos = this->segments[b1];
				newdelay = this->ReadVariableLength(chandata.playpos);
				if (newdelay == 0) {
					continue;
				}
				return newdelay;
			}

			/* Command 0xFD, return from segment to master track */
			if (b1 == MPSMIDIST_SEGMENT_RETURN) {
				chandata.playpos = chandata.returnpos;
				chandata.returnpos = 0;
				newdelay = this->ReadVariableLength(chandata.playpos);
				if (newdelay == 0) {
					continue;
				}
				return newdelay;
			}

			/* Command 0xFF, end of song */
			if (b1 == MPSMIDIST_ENDSONG) {
				this->shouldplayflag = false;
				return 0;
			}

			/* Regular MIDI channel message status byte */
			if (b1 >= 0x80) {
				/* Save the status byte as running status for the channel
				 * and read another byte for first parameter to command */
				chandata.running_status = b1;
				b1 = this->songdata[chandata.playpos++];
			}

			switch (chandata.running_status & 0xF0) {
				case MIDIST_NOTEOFF:
				case MIDIST_NOTEON:
					b2 = this->songdata[chandata.playpos++];
					if (b2 != 0) {
						/* Note on, read velocity and scale according to rules */
						int16_t velocity;
						if (channel == 9) {
							/* Percussion channel, fixed velocity scaling not in the table */
							velocity = (int16_t)b2 * 0x50;
						} else {
							/* Regular channel, use scaling from table */
							velocity = b2 * programvelocities[chandata.cur_program];
						}
						b2 = (velocity / 128) & 0x00FF;
						AddMidiData(outblock, MIDIST_NOTEON + channel, b1, b2);
					} else {
						/* Note off */
						AddMidiData(outblock, MIDIST_NOTEON + channel, b1, 0);
					}
					break;
				case MIDIST_CONTROLLER:
					b2 = this->songdata[chandata.playpos++];
					if (b1 == MIDICT_MODE_MONO) {
						/* Unknown what the purpose of this is.
						 * Occurs in "Can't get There from Here" and in "Aliens Ate my Railway" a few times each.
						 * Possibly intended to give hints to other (non-GM) music drivers decoding the song.
						 */
						break;
					} else if (b1 == 0) {
						/* Standard MIDI controller 0 is "bank select", override meaning to change tempo.
						 * This is not actually used in any of the original songs. */
						if (b2 != 0) {
							this->current_tempo = ((int)b2) * 48 / 60;
						}
						break;
					} else if (b1 == MIDICT_EFFECTS1) {
						/* Override value of this controller, default mapping is Reverb Send Level according to MMA RP-023.
						 * Unknown what the purpose of this particular value is. */
						b2 = 30;
					}
					AddMidiData(outblock, MIDIST_CONTROLLER + channel, b1, b2);
					break;
				case MIDIST_PROGCHG:
					if (b1 == 0x7E) {
						/* Program change to "Applause" is originally used
						 * to cause the song to loop, but that gets handled
						 * separately in the output driver here.
						 * Just end the song. */
						this->shouldplayflag = false;
						break;
					}
					/* Used for note velocity scaling lookup */
					chandata.cur_program = b1;
					/* Two programs translated to a third, this is likely to
					 * provide three different velocity scalings of "brass". */
					if (b1 == 0x57 || b1 == 0x3F) {
						b1 = 0x3E;
					}
					AddMidiData(outblock, MIDIST_PROGCHG + channel, b1);
					break;
				case MIDIST_PITCHBEND:
					b2 = this->songdata[chandata.playpos++];
					AddMidiData(outblock, MIDIST_PITCHBEND + channel, b1, b2);
					break;
				default:
					break;
			}

			newdelay = this->ReadVariableLength(chandata.playpos);
		} while (newdelay == 0);

		return newdelay;
	}

	/**
	 * Play one frame of data into a block.
	 */
	bool PlayFrame(MidiFile::DataBlock &block)
	{
		/* Update tempo/ticks counter */
		this->tempo_ticks -= this->current_tempo;
		if (this->tempo_ticks > 0) {
			return true;
		}
		this->tempo_ticks += TEMPO_RATE;

		/* Look over all channels, play those active */
		for (int ch = 0; ch < 16; ch++) {
			Channel &chandata = this->channels[ch];
			if (chandata.playpos != 0) {
				if (chandata.delay == 0) {
					chandata.delay = this->PlayChannelFrame(block, ch);
				}
				chandata.delay--;
			}
		}

		return this->shouldplayflag;
	}

	/**
	 * Perform playback of whole song.
	 */
	bool PlayInto()
	{
		/* Tempo seems to be handled as TEMPO_RATE = 148 ticks per second.
		 * Use this as the tickdiv, and define the tempo to be somewhat less than one second (1M microseconds) per quarter note.
		 * This value was found experimentally to give a very close approximation of the correct playback speed.
		 * MIDI software loading exported files will show a bogus tempo, but playback will be correct. */
		this->target.tickdiv = TEMPO_RATE;
		this->target.tempos.push_back(MidiFile::TempoChange(0, 980500));

		/* Initialize playback simulation */
		this->RestartSong();
		this->shouldplayflag = true;
		this->current_tempo = (int32_t)this->initial_tempo * 24 / 60;
		this->tempo_ticks = this->current_tempo;

		/* Always reset percussion channel to program 0 */
		this->target.blocks.push_back(MidiFile::DataBlock());
		AddMidiData(this->target.blocks.back(), MIDIST_PROGCHG + 9, 0x00);

		/* Technically should be an endless loop, but having
		 * a maximum (about 10 minutes) avoids getting stuck,
		 * in case of corrupted data. */
		for (uint32_t tick = 0; tick < 100000; tick += 1) {
			this->target.blocks.push_back(MidiFile::DataBlock());
			auto &block = this->target.blocks.back();
			block.ticktime = tick;
			if (!this->PlayFrame(block)) {
				break;
			}
		}
		return true;
	}
};
/** Frames/ticks per second for music playback */
const int MpsMachine::TEMPO_RATE = 148;
/** Base note velocities for various GM programs */
const byte MpsMachine::programvelocities[128] = {
	100, 100, 100, 100, 100,  90, 100, 100, 100, 100, 100,  90, 100, 100, 100, 100,
	100, 100,  85, 100, 100, 100, 100, 100, 100, 100, 100, 100,  90,  90, 110,  80,
	100, 100, 100,  90,  70, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100,  90, 100, 100, 100, 100, 100, 100, 120, 100, 100, 100, 120, 100, 127,
	100, 100,  90, 100, 100, 100, 100, 100, 100,  95, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 115, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
};

/**
 * Create MIDI data from song data for the original Microprose music drivers.
 * @param data pointer to block of data
 * @param length size of data in bytes
 * @return true if the data could be loaded
 */
bool MidiFile::LoadMpsData(const byte *data, size_t length)
{
	_midifile_instance = this;

	MpsMachine machine(data, length, *this);
	return machine.PlayInto() && FixupMidiData(*this);
}

bool MidiFile::LoadSong(const MusicSongInfo &song)
{
	switch (song.filetype) {
		case MTT_STANDARDMIDI:
			return this->LoadFile(song.filename);
		case MTT_MPSMIDI:
		{
			size_t songdatalen = 0;
			byte *songdata = GetMusicCatEntryData(song.filename, song.cat_index, songdatalen);
			if (songdata != nullptr) {
				bool result = this->LoadMpsData(songdata, songdatalen);
				free(songdata);
				return result;
			} else {
				return false;
			}
		}
		default:
			NOT_REACHED();
	}
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

	_midifile_instance = this;

	other.blocks.clear();
	other.tempos.clear();
	other.tickdiv = 0;
}

static void WriteVariableLen(FILE *f, uint32_t value)
{
	if (value <= 0x7F) {
		byte tb = value;
		fwrite(&tb, 1, 1, f);
	} else if (value <= 0x3FFF) {
		byte tb[2];
		tb[1] =  value & 0x7F;         value >>= 7;
		tb[0] = (value & 0x7F) | 0x80; value >>= 7;
		fwrite(tb, 1, sizeof(tb), f);
	} else if (value <= 0x1FFFFF) {
		byte tb[3];
		tb[2] =  value & 0x7F;         value >>= 7;
		tb[1] = (value & 0x7F) | 0x80; value >>= 7;
		tb[0] = (value & 0x7F) | 0x80; value >>= 7;
		fwrite(tb, 1, sizeof(tb), f);
	} else if (value <= 0x0FFFFFFF) {
		byte tb[4];
		tb[3] =  value & 0x7F;         value >>= 7;
		tb[2] = (value & 0x7F) | 0x80; value >>= 7;
		tb[1] = (value & 0x7F) | 0x80; value >>= 7;
		tb[0] = (value & 0x7F) | 0x80; value >>= 7;
		fwrite(tb, 1, sizeof(tb), f);
	}
}

/**
 * Write a Standard MIDI File containing the decoded music.
 * @param filename Name of file to write to
 * @return True if the file was written to completion
 */
bool MidiFile::WriteSMF(const std::string &filename)
{
	FILE *f = FioFOpenFile(filename, "wb", Subdirectory::NO_DIRECTORY);
	if (!f) {
		return false;
	}

	/* SMF header */
	const byte fileheader[] = {
		'M', 'T', 'h', 'd',     // block name
		0x00, 0x00, 0x00, 0x06, // BE32 block length, always 6 bytes
		0x00, 0x00,             // writing format 0 (all in one track)
		0x00, 0x01,             // containing 1 track (BE16)
		(byte)(this->tickdiv >> 8), (byte)this->tickdiv, // tickdiv in BE16
	};
	fwrite(fileheader, sizeof(fileheader), 1, f);

	/* Track header */
	const byte trackheader[] = {
		'M', 'T', 'r', 'k', // block name
		0, 0, 0, 0,         // BE32 block length, unknown at this time
	};
	fwrite(trackheader, sizeof(trackheader), 1, f);
	/* Determine position to write the actual track block length at */
	size_t tracksizepos = ftell(f) - 4;

	/* Write blocks in sequence */
	uint32_t lasttime = 0;
	size_t nexttempoindex = 0;
	for (size_t bi = 0; bi < this->blocks.size(); bi++) {
		DataBlock &block = this->blocks[bi];
		TempoChange &nexttempo = this->tempos[nexttempoindex];

		uint32_t timediff = block.ticktime - lasttime;

		/* Check if there is a tempo change before this block */
		if (nexttempo.ticktime < block.ticktime) {
			timediff = nexttempo.ticktime - lasttime;
		}

		/* Write delta time for block */
		lasttime += timediff;
		bool needtime = false;
		WriteVariableLen(f, timediff);

		/* Write tempo change if there is one */
		if (nexttempo.ticktime <= block.ticktime) {
			byte tempobuf[6] = { MIDIST_SMF_META, 0x51, 0x03, 0, 0, 0 };
			tempobuf[3] = (nexttempo.tempo & 0x00FF0000) >> 16;
			tempobuf[4] = (nexttempo.tempo & 0x0000FF00) >>  8;
			tempobuf[5] = (nexttempo.tempo & 0x000000FF);
			fwrite(tempobuf, sizeof(tempobuf), 1, f);
			nexttempoindex++;
			needtime = true;
		}
		/* If a tempo change occurred between two blocks, rather than
		 * at start of this one, start over with delta time for the block. */
		if (nexttempo.ticktime < block.ticktime) {
			/* Start loop over at same index */
			bi--;
			continue;
		}

		/* Write each block data command */
		byte *dp = block.data.data();
		while (dp < block.data.data() + block.data.size()) {
			/* Always zero delta time inside blocks */
			if (needtime) {
				fputc(0, f);
			}
			needtime = true;

			/* Check message type and write appropriate number of bytes */
			switch (*dp & 0xF0) {
				case MIDIST_NOTEOFF:
				case MIDIST_NOTEON:
				case MIDIST_POLYPRESS:
				case MIDIST_CONTROLLER:
				case MIDIST_PITCHBEND:
					fwrite(dp, 1, 3, f);
					dp += 3;
					continue;
				case MIDIST_PROGCHG:
				case MIDIST_CHANPRESS:
					fwrite(dp, 1, 2, f);
					dp += 2;
					continue;
			}

			/* Sysex needs to measure length and write that as well */
			if (*dp == MIDIST_SYSEX) {
				fwrite(dp, 1, 1, f);
				dp++;
				byte *sysexend = dp;
				while (*sysexend != MIDIST_ENDSYSEX) sysexend++;
				ptrdiff_t sysexlen = sysexend - dp;
				WriteVariableLen(f, sysexlen);
				fwrite(dp, 1, sysexend - dp, f);
				dp = sysexend + 1;
				continue;
			}

			/* Fail for any other commands */
			fclose(f);
			return false;
		}
	}

	/* End of track marker */
	static const byte track_end_marker[] = { 0x00, MIDIST_SMF_META, 0x2F, 0x00 };
	fwrite(&track_end_marker, sizeof(track_end_marker), 1, f);

	/* Fill out the RIFF block length */
	size_t trackendpos = ftell(f);
	fseek(f, tracksizepos, SEEK_SET);
	uint32_t tracksize = (uint32_t)(trackendpos - tracksizepos - 4); // blindly assume we never produce files larger than 2 GB
	tracksize = TO_BE32(tracksize);
	fwrite(&tracksize, 4, 1, f);

	fclose(f);
	return true;
}

/**
 * Get the name of a Standard MIDI File for a given song.
 * For songs already in SMF format, just returns the original.
 * Otherwise the song is converted, written to a temporary-ish file, and the written filename is returned.
 * @param song Song definition to query
 * @return Full filename string, empty string if failed
 */
std::string MidiFile::GetSMFFile(const MusicSongInfo &song)
{
	if (song.filetype == MTT_STANDARDMIDI) {
		std::string filename = FioFindFullPath(Subdirectory::BASESET_DIR, song.filename);
		if (!filename.empty()) return filename;
		filename = FioFindFullPath(Subdirectory::OLD_GM_DIR, song.filename);
		if (!filename.empty()) return filename;

		return std::string();
	}

	if (song.filetype != MTT_MPSMIDI) return std::string();

	char basename[MAX_PATH];
	{
		const char *fnstart = strrchr(song.filename.c_str(), PATHSEPCHAR);
		if (fnstart == nullptr) {
			fnstart = song.filename.c_str();
		} else {
			fnstart++;
		}

		/* Remove all '.' characters from filename */
		char *wp = basename;
		for (const char *rp = fnstart; *rp != '\0'; rp++) {
			if (*rp != '.') *wp++ = *rp;
		}
		*wp++ = '\0';
	}

	std::string tempdirname = FioGetDirectory(Searchpath::SP_AUTODOWNLOAD_DIR, Subdirectory::BASESET_DIR);
	tempdirname += basename;
	AppendPathSeparator(tempdirname);
	FioCreateDirectory(tempdirname);

	std::string output_filename = tempdirname + std::to_string(song.cat_index) + ".mid";

	if (FileExists(output_filename)) {
		/* If the file already exists, assume it's the correct decoded data */
		return output_filename;
	}

	byte *data;
	size_t datalen;
	data = GetMusicCatEntryData(song.filename, song.cat_index, datalen);
	if (data == nullptr) return std::string();

	MidiFile midifile;
	if (!midifile.LoadMpsData(data, datalen)) {
		free(data);
		return std::string();
	}
	free(data);

	if (midifile.WriteSMF(output_filename)) {
		return output_filename;
	} else {
		return std::string();
	}
}


static bool CmdDumpSMF(byte argc, char *argv[])
{
	if (argc == 0) {
		IConsolePrint(CC_HELP, "Write the current song to a Standard MIDI File. Usage: 'dumpsmf <filename>'.");
		return true;
	}
	if (argc != 2) {
		IConsolePrint(CC_WARNING, "You must specify a filename to write MIDI data to.");
		return false;
	}

	if (_midifile_instance == nullptr) {
		IConsolePrint(CC_ERROR, "There is no MIDI file loaded currently, make sure music is playing, and you're using a driver that works with raw MIDI.");
		return false;
	}

	std::string filename = fmt::format("{}{}", FiosGetScreenshotDir(), argv[1]);
	IConsolePrint(CC_INFO, "Dumping MIDI to '{}'.", filename);

	if (_midifile_instance->WriteSMF(filename)) {
		IConsolePrint(CC_INFO, "File written successfully.");
		return true;
	} else {
		IConsolePrint(CC_ERROR, "An error occurred writing MIDI file.");
		return false;
	}
}

static void RegisterConsoleMidiCommands()
{
	static bool registered = false;
	if (!registered) {
		IConsole::CmdRegister("dumpsmf", CmdDumpSMF);
		registered = true;
	}
}

MidiFile::MidiFile()
{
	RegisterConsoleMidiCommands();
}

MidiFile::~MidiFile()
{
	if (_midifile_instance == this) {
		_midifile_instance = nullptr;
	}
}

